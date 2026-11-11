import readline from 'node:readline'
import { randomUUID } from 'node:crypto'
import Anthropic from '@anthropic-ai/sdk'
import { resolveAnthropicCredentials, resolveZhipuApiKey, resolveOpenAiCompatKey } from './auth.mjs'
import { classifyUserIntent, assessEvidence } from './laya.mjs'
import { retrieveEvidence } from './jev_read.mjs'
import { createIssue } from './issues.mjs'
import { resolveWireModel, anthropicBaseUrl, omitToolChoice } from './modelConfig.mjs'
import { runHooks } from './hookplane.mjs'
import { appendEntry, transcriptToAnthropicMessages, transcriptToOpenAIMessages, makeUserPayload, makeAssistantPayload, makeToolResultPayload, applyEpistemicFilter } from './transcript.mjs'
import { evaluatePermission } from './permissions.mjs'
import { executeBuiltinTool, anthropicToolDefinitions, openAICompatToolDefinitions, LOCAL_CORE_TOOLS, EXPLORE_ONLY_TOOLS, dynamicTools, getMcpToolDefinitions, resetExploreReadCache as resetExploreReadCacheTools } from './tools.mjs'
import { streamOpenAIChatCompletion, isAbortError } from './openaiCompat.mjs'
import { withTransaction } from './store.mjs'
import { upsertSummary } from './store.mjs'
import { upsertPinnedContext } from './store.mjs'
import { getPhase, setPhase, canTransition, validatePlanSchema } from './workflow.mjs'
import { countPlanScenarios } from './workflow.mjs'
import { getRedTestEvidence, recordRedTestEvidence, getOriginalRedEvidence, getCurrentGreenEvidence, normalizeLegacyPhase, hasCurrentSafetyChecks } from './workflow.mjs'
import { runSafetyChecks } from './safety-gate.mjs'
import { collectBranchCoverage } from './branch-coverage-collector.mjs'
import fs from 'node:fs'
import path from 'node:path'
import { buildJevSystem2Messages } from './compact.mjs'
import { ingestObservation } from './jev_write.mjs'

// ─── Dynamic tool prompt injection ───────────────────────────

function getMandatoryToolInstructions(phase) {
  const instructions = []
  for (const [, config] of dynamicTools) {
    if (config.phases?.mandatory_in?.includes(phase) && config.prompt?.mandatory_instruction) {
      instructions.push(`- MANDATORY: ${config.prompt.mandatory_instruction}`)
    }
  }
  if (instructions.length === 0) return ''
  return `- After tests pass, you MUST run these mandatory safety checks. Do NOT skip them. If any fail, fix the issues and re-run.\n${instructions.join('\n')}\n`
}

function getAvailableToolHints() {
  const hints = []
  for (const [, config] of dynamicTools) {
    if (config.prompt?.available_hint) {
      hints.push(`- ${config.name}: ${config.prompt.available_hint}`)
    }
  }
  if (hints.length === 0) return ''
  return `\n# Safety & Analysis Tools\nYou have access to these tools for code review and safety analysis:\n${hints.join('\n')}\n\n`
}

// ─── Domain context notes for intent-shaped system prompts ───

/**
 * Per-domain preamble injected into the system prompt when Laya classifies
 * the user's intent. Used by buildSystemPrompt when intentMeta is present.
 * Only active in idle/explore/default phases — not in implement/test.
 */
const DOMAIN_CONTEXT_NOTES = {
  energy_data:    'This task involves energy telemetry normalization. Preserve ODS-E field names and units exactly. Do not rename or reformat telemetry fields without verifying the full OEM schema mapping.',
  test_debug:     'A test or build is failing. Diagnose the root cause before proposing a fix. Do not suppress errors, skip assertions, or mark tests as passing without running them.',
  architectural:  'This task crosses subsystem boundaries. Preserve existing interfaces and verify all call sites before modifying function signatures or module contracts.',
  infrastructure: 'This task affects deployment configuration. Validate environment variable names and resource names against the documented naming conventions before writing changes.',
}

// ─── Phase system prompts ─────────────────────────────────────

function getPathCoverage(db, conversationId) {
  const rows = db.prepare(`SELECT detail FROM events WHERE conversation_id = ? AND event_type = 'path_coverage' ORDER BY id DESC`).all(conversationId)
  for (const r of rows) {
    try {
      const d = JSON.parse(r.detail || '{}')
      for (const key of ['pathCoverage','path_coverage','coverage']) {
        const n = Number(d?.[key])
        if (Number.isFinite(n)) return n
      }
    } catch {}
  }
  return 0
}

const PHASE_SYSTEM = {
  explore: (cwd, provider, model, date, os, shell, onaInstructions) => `You are ona, an autonomous CLI agent.

You are in EXPLORE phase. Your ONLY job is to find and read files relevant to the task.
- Use Glob, Grep, and Read to explore the codebase
- Read every file that could be relevant to the task
- Do NOT write plans, suggest steps, ask questions, or produce any output other than tool calls
- When you have read everything relevant, output EXACTLY this single line and nothing else:
  Exploration complete.

# Environment
- Working directory: ${cwd}
- Platform: ${os} | Shell: ${shell} | Date: ${date}
- Provider: ${provider} | Model: ${model}${onaInstructions ? `\n\n# Project Instructions\n${onaInstructions}` : ''}`,

  diagnose: (cwd, provider, model, date, os, shell, onaInstructions) => `You are ona, an autonomous CLI agent.

You are in DIAGNOSE phase. Analyze evidence and record a diagnosis before issuing any code changes.
- Identify the diagnosed root cause.
- The SDLC harness records diagnosis evidence for you.
# Environment
- Working directory: ${cwd}\n- Platform: ${os} | Shell: ${shell} | Date: ${date}\n- Provider: ${provider} | Model: ${model}${onaInstructions ? `\n\n# Project Instructions\n${onaInstructions}` : ''}`,

  create_issue: (cwd, provider, model, date, os, shell, onaInstructions) => `You are ona, an autonomous CLI agent.

You are in CREATE_ISSUE phase. Prepare a concise issue record from the diagnosis.
- Include observed behavior, expected behavior, diagnosis, and acceptance criteria.
# Environment\n- Working directory: ${cwd}\n- Platform: ${os} | Shell: ${shell} | Date: ${date}\n- Provider: ${provider} | Model: ${model}${onaInstructions ? `\n\n# Project Instructions\n${onaInstructions}` : ''}`,

  create_tests: (cwd, provider, model, date, os, shell, onaInstructions) => `You are ona, an autonomous CLI agent.

You are in CREATE_TESTS phase. Follow TDD: create and run tests that reproduce or confirm expected behavior before implementation.
- Confirm the RED state expected by the approved plan.
- Do not edit production code in this phase.
# Environment\n- Working directory: ${cwd}\n- Platform: ${os} | Shell: ${shell} | Date: ${date}\n- Provider: ${provider} | Model: ${model}${onaInstructions ? `\n\n# Project Instructions\n${onaInstructions}` : ''}`,

  verify: (cwd, provider, model, date, os, shell, onaInstructions) => `You are ona, an autonomous CLI agent.

You are in VERIFY phase. Run and review the relevant tests for GREEN evidence.
- Confirm the implementation behavior before safety checks.
# Environment\n- Working directory: ${cwd}\n- Platform: ${os} | Shell: ${shell} | Date: ${date}\n- Provider: ${provider} | Model: ${model}${onaInstructions ? `\n\n# Project Instructions\n${onaInstructions}` : ''}`,

  safety: (cwd, provider, model, date, os, shell, onaInstructions) => `You are ona, an autonomous CLI agent.

You are in SAFETY phase. Evaluate the mandatory safety checkers and path coverage.
- Do not commit until all gates pass.
# Environment\n- Working directory: ${cwd}\n- Platform: ${os} | Shell: ${shell} | Date: ${date}\n- Provider: ${provider} | Model: ${model}${onaInstructions ? `\n\n# Project Instructions\n${onaInstructions}` : ''}`,

  confirm_fix: (cwd, provider, model, date, os, shell, onaInstructions) => `You are ona, an autonomous CLI agent.

You are in CONFIRM_FIX phase. Confirm the completed fix before the terminal handoff.
# Environment\n- Working directory: ${cwd}\n- Platform: ${os} | Shell: ${shell} | Date: ${date}\n- Provider: ${provider} | Model: ${model}${onaInstructions ? `\n\n# Project Instructions\n${onaInstructions}` : ''}`,

  planning: (cwd, provider, model, date, os, shell, onaInstructions) => `You are ona, an autonomous CLI agent.

You are in PLANNING phase. File summaries from exploration are provided below.
Write your plan based ONLY on those summaries. Do NOT use any tools.

Write your plan using EXACTLY this template and nothing else:

## Objective
<one sentence describing what you will produce>

## Plan
1. <exact command, script invocation, or file write — not vague intent>
2. <exact command, script invocation, or file write>
...

Rules: every step must name exact paths and commands extracted from the summaries. Never use "review", "reference", or "based on documentation" as a step.

## Behavioral Specification
Given <precondition>, When <action>, Then <observable outcome>.

## Coverage Matrix
| Scenario | Test File |
| --- | --- |
| <behavior> | <actual regression path> |

## Proof of Success
<specific file or output that will exist when done>

# Environment
- Working directory: ${cwd}
- Platform: ${os} | Shell: ${shell} | Date: ${date}
- Provider: ${provider} | Model: ${model}${onaInstructions ? `\n\n# Project Instructions\n${onaInstructions}` : ''}`,

  implement: (cwd, provider, model, date, os, shell, onaInstructions) => `You are ona, an autonomous CLI agent.

You are in IMPLEMENT phase. Execute the approved plan exactly.
- Use Write or Edit to create/modify files
- Use Bash to run commands if needed
- When all implementation work is done, write a one-line summary as your final response: "Done: <what you did>"
- Do not ask questions. Do not explain. Just implement.

# Environment
- Working directory: ${cwd}
- Platform: ${os} | Shell: ${shell} | Date: ${date}
- Provider: ${provider} | Model: ${model}${onaInstructions ? `\n\n# Project Instructions\n${onaInstructions}` : ''}`,

  test: (cwd, provider, model, date, os, shell, onaInstructions) => {
    const mandatoryChecks = getMandatoryToolInstructions('test')
    return `You are ona, an autonomous CLI agent.

You are in TEST phase.
- Run the project tests using Bash
- If tests fail, fix the code and re-run
${mandatoryChecks}
- When all tests pass AND all mandatory checks pass, write a one-line summary as your final response: "Tests passed: <what was verified>"
- Do not ask questions. Do not explain. Just run the tests and checks.

# Environment
- Working directory: ${cwd}
- Platform: ${os} | Shell: ${shell} | Date: ${date}
- Provider: ${provider} | Model: ${model}${onaInstructions ? `\n\n# Project Instructions\n${onaInstructions}` : ''}`
  },

  default: (cwd, provider, model, date, os, shell, onaInstructions) => {
    const availableHints = getAvailableToolHints()
    return `You are ona, an interactive CLI agent that helps users with software engineering tasks.

- Proactively use tools. Do not ask for permission.
- Be concise. Lead with actions, not reasoning.
- Use Read to understand files before editing.
- For discovery — listing files, finding paths, searching file contents — use Glob and Grep, not Bash. Reserve Bash for actual execution: running commands, tests, installs, git operations.
${availableHints}
# Environment
- Working directory: ${cwd}
- Platform: ${os} | Shell: ${shell} | Date: ${date}
- Provider: ${provider} | Model: ${model}${onaInstructions ? `\n\n# Project Instructions\n${onaInstructions}` : ''}`
  },
}

// ─── Phase user message templates ────────────────────────────

export function buildExploreUserMessage(userRequest, cwd = process.cwd()) {
  const base = path.resolve(cwd || process.cwd())
  const repoMapPath = path.join(base, 'docs', 'MAP.md')
  const cacheMapPath = path.join(base, '.ona', 'map', 'map.md')
  const hasRepoMap = fs.existsSync(repoMapPath)
  const hasCacheMap = !hasRepoMap && fs.existsSync(cacheMapPath)
  let registryInstruction
  if (hasRepoMap) {
    registryInstruction = '1. Read docs/MAP.md first to identify the required entry point, contract, and relevant context registry entries.'
  } else if (hasCacheMap) {
    registryInstruction = '1. No docs/MAP.md exists; read .ona/map/map.md first as a compact exploration cache. Treat it as cached guidance, not canonical source documentation.'
  } else {
    registryInstruction = '1. No docs/MAP.md or .ona/map/map.md exists. Build a tiny exploration index in memory (high-signal paths, roots, ignore rules, relevant symbols). You may promote it to .ona/map/map.md only after the operator explicitly approves persisting it (never to the project root).'
  }
  return `Task: ${userRequest}

Explore using the AGENTS.md Phase 1 procedure for this specific task:

${registryInstruction}
2. Do not re-read a file or repeat a search after no new information. If a file has already been logged in this exploration and you do not have a new file-specific reason, stop and hand off.
3. Use grep and glob to find existing patterns only with task keywords or registry-suggested narrow patterns. Whole-file sweeps are prohibited. Unbounded patterns such as .+, .*, ^.?$, or empty strings are invalid.
4. Trace the execution path end to end and read only files directly implicated by the task.
5. If no matching context is found for this task, report that the registry has no corresponding entry, do not broaden into unrelated repository data, and prepare for exploration exhausted handoff. "No matching context in current repo; provide clarification or expanded scope."
6. Compare the documentation, code, configuration, and tests for that path; identify discrepancies before proposing changes.
7. Do not read broad data files, archived docs, or unrelated service directories unless they are specifically named in the execution path.

IMPORTANT: Your task is "${userRequest}". Only read files related to this task.

Use the working directory as the base for all tool calls.
Do NOT guess paths. Only read files that appear in glob/grep results.
When done reading, output EXACTLY:
  Exploration complete.`
}

function buildDiagnoseUserMessage(summaries,userRequest){
  return 'DIAGNOSE the observed behavior using the exploration evidence. State observed behavior, expected behavior and the evidenced cause. Do not propose an implementation plan yet.\n\nTask: '+userRequest+'\n\n'+JSON.stringify(summaries)
}

function buildPlanningUserMessage(summaries, userRequest) {
  let fileSection
  if (!summaries || summaries.length === 0) {
    fileSection = '(no files were successfully read during exploration)'
  } else {
    fileSection = summaries.map(({ path, header, summary }) => {
      const parts = [`### ${path}`]
      if (summary) parts.push(`**Relevance:** ${summary}`)
      if (header) parts.push('**File header (first 50 lines):**\n```\n' + header + '\n```')
      return parts.join('\n')
    }).join('\n\n')
  }

  return `Exploration and summarization are complete. Here is what each file contains that is relevant to the task:

${fileSection}

Original task: ${userRequest}

Write your plan using ONLY the content above. Do NOT reference files not listed here. Do NOT use any tools.

## Issue Reference
<the recorded issue number>

## Objective
<one sentence: what you will produce>

## Plan
1. <concrete step — must specify exact file path, command, or code change>
2. <concrete step>

RULES FOR PLAN STEPS:
- Every step must be executable: name the exact script, command, or file to write
- If running a script, include the exact invocation with arguments drawn from the summaries above
- If writing a file, name the target path and describe its contents
- NEVER use vague verbs: "review", "understand", "reference", "use as reference", "based on the documentation"

## Behavioral Specification
Given <precondition>, When <action>, Then <observable outcome>.

## Coverage Matrix
| Scenario | Test File |
| --- | --- |
| <behavior> | <actual regression path> |

## Proof of Success
<specific file or output that will exist when done>

Do NOT use any tools. Write the plan only.`
}

function buildCreateTestUserMessage(planContent) {
  return `Per TDD (AGENTS.md Phase 4), create or update behavioral tests now BEFORE modifying production code. Do this without assuming any prior plan approval beyond the current gate.

${planContent}

Write the behavioral test and run it using Bash to confirm it is demonstrably RED (failing for the diagnosed reason). End with "Tests failed: <reason>".`
}

function buildImplementUserMessage(planContent) {
  return `Approved plan:

${planContent}

Implement the plan now using Write, Edit, and Bash as needed.`
}

function buildTestUserMessage() {
  return `Implementation accepted. You MUST run the tests now using Bash before doing anything else. Execute the appropriate test command for this project (e.g. python hello_world.py, pytest, npm test, etc.).`
}

function buildImplementSummaryPrompt(toolResults) {
  return `You just completed the implementation. Here is what actually happened:

${toolResults}

Fill in this summary using ONLY the information above — do not invent anything:

## Files Changed
- <file path>: <what was done to it>

## Result
<one sentence: what was produced>`
}

function buildTestSummaryPrompt(toolResults) {
  return `You just ran the tests. Here is what actually happened:

${toolResults}

Fill in this summary using ONLY the information above — do not invent anything:

## Tests Run
<command that was executed>

## Result
<one sentence: passed or failed, what was verified>`
}

export function buildSystemPrompt(cwd, provider, model, phase, onaInstructions, injectedTools = null, intentMeta = null) {
  const os = process.platform === 'darwin' ? 'macOS' : process.platform === 'win32' ? 'Windows' : 'Linux'
  const shell = process.env.SHELL?.split('/').pop() || 'sh'
  const date = new Date().toISOString().split('T')[0]
  const ticks = String.fromCharCode(96, 96, 96)
  const LANGUAGE_LOCK = 'Always respond in English.'

  const builder = PHASE_SYSTEM[phase] || PHASE_SYSTEM.default
  let prompt = builder(cwd, provider, model, date, os, shell, onaInstructions)

  // #22: Explicit grounded facts prevent reasoning models from spending tokens
  // debating their own awareness of date, cwd, or environment.
  prompt += `\n\n[GROUNDED FACTS]\nDate: ${date}\nWorking directory: ${cwd}\nPlatform: ${os} | Shell: ${shell}`
  // AGENTS.md Commandment 11 + Phase 3: issue-first auth / fallback guidance.
  prompt += `\n\n[ISSUE WORKFLOW POLICY]\nCreate or reference a GitHub issue before code changes whenever possible. Authenticate GitHub operations through Git Credential Manager. If no cached credential works, fall back to creating a markdown file under .issue/<issue-name>-<date>.md at the repository root. Never search for, read, or log token files.`

  // Inject intent-shaped task context preamble for idle/explore/default phases.
  // Not injected in implement/test/planning — those phases use generated template
  // messages that carry no real user intent signal.
  if (intentMeta && ['idle', 'explore', 'default'].includes(phase)) {
    const domainNote = DOMAIN_CONTEXT_NOTES[intentMeta.task_domain] || ''
    const contextLines = [
      '[TASK CONTEXT — System 1]',
      `Domain: ${intentMeta.task_domain} | Scope: ${intentMeta.task_scope}`,
    ]
    if (intentMeta.requires_domain_context) {
      contextLines.push('Domain context: required — apply ODSE schema invariants and energy asset constraints precisely.')
    }
    if (domainNote) contextLines.push(domainNote)
    contextLines.push('--------------------------------------------------')
    prompt = contextLines.join('\n') + '\n\n' + prompt
  }

  // Pin output language for multilingual models (e.g. Qwen) that otherwise code-switch.
  if (!prompt.includes(LANGUAGE_LOCK)) {
    const nl = prompt.indexOf('\n')
    prompt = nl === -1
      ? `${prompt}\n\n${LANGUAGE_LOCK}`
      : `${prompt.slice(0, nl)}\n\n${LANGUAGE_LOCK}${prompt.slice(nl)}`
  }

  if (injectedTools && injectedTools.length > 0) {
    prompt += `\n\n# MANDATORY: Tool Calling Format
Your environment requires JSON tool calls in markdown blocks:
${ticks}json
{"tool": "ToolName", "input": {"param": "value"}}
${ticks}

Available Tools:
${injectedTools.map(t => `- ${t.function.name}: ${t.function.description} (Schema: ${JSON.stringify(t.function.parameters.properties)})`).join('\n')}`
  }

  return prompt
}

function buildXmlToolInstructions(tools, phase) {
  if (phase === 'planning' || !tools || tools.length === 0) return ''

  const KEY_TOOLS = ['Bash', 'Read', 'Glob']
  const examples = tools
    .filter(t => KEY_TOOLS.includes(t.function?.name))
    .map(t => {
      const fn  = t.function
      const req = fn.parameters?.required || []
      const args = Object.entries(fn.parameters?.properties || {})
        .filter(([k]) => req.includes(k))
        .map(([k, v]) => `    "${k}": "${v.type === 'integer' ? '0' : '...'}"`)
        .join(',\n')
      return `[TOOL_CALL]\n{"name": "${fn.name}", "arguments": {\n${args}\n}}\n[/TOOL_CALL]`
    })
    .join('\n\n')

  const allNames = tools.filter(t => t.function?.name).map(t => t.function.name).join(', ')

  return `\n\n# Tool Calling Format\n\nTo call a tool, emit a [TOOL_CALL] block containing a valid JSON object. Do NOT use XML tags inside the block.\n\nExamples:\n\n${examples}\n\nAll available tools: ${allNames}\n\nRules:\n- Emit strictly [TOOL_CALL] {"name": "...", "arguments": {...}} [/TOOL_CALL]. Do NOT use <arguments> or XML tags.\n- Escape all double quotes inside command strings (e.g., "echo \\"---LOG---\\"").\n- One tool call per response. After tool use is complete, write your final answer as plain text.`
}


function filterToolsByPhase(tools, phase) {
  if (phase === 'explore') {
    return tools.filter(t => {
      const name = t.name || t.function?.name
      return EXPLORE_ONLY_TOOLS.has(name)
    })
  }
  if (phase === 'planning') {
    return []
  }
  return tools
}

const sessionTokens = new Map()

export function getSessionTokens(sessionId) {
  return sessionTokens.get(sessionId) || { input: 0, output: 0, calls: 0 }
}

function trackTokens(sessionId, inputTokens, outputTokens) {
  const prev = sessionTokens.get(sessionId) || { input: 0, output: 0, calls: 0 }
  sessionTokens.set(sessionId, {
    input: prev.input + (inputTokens || 0),
    output: prev.output + (outputTokens || 0),
    calls: prev.calls + 1,
  })
}

function makeAnthropicClient(cred) {
  const baseURL = anthropicBaseUrl()
  if (cred.mode === 'bearer') return new Anthropic({ authToken: cred.secret, apiKey: null, baseURL, defaultHeaders: { 'anthropic-beta': 'oauth-2025-04-20' } })
  if (cred.mode === 'api_key') return new Anthropic({ apiKey: cred.secret, baseURL })
  throw new Error('No credentials for the active model provider')
}

function activeProvider(settings) {
  return settings?.model_config?.provider || 'claude_code_subscription'
}

async function toolsForProvider(provider, settings, nativeToolSupport = true) {
  const coreOnly = provider === 'lm_studio_local' || (provider === 'ollama' && !nativeToolSupport)
  const mcpTools = await getMcpToolDefinitions(settings)

  const anthropicBase = coreOnly
    ? anthropicToolDefinitions().filter(t => LOCAL_CORE_TOOLS.has(t.name))
    : anthropicToolDefinitions()

  const openaiBase = coreOnly
    ? openAICompatToolDefinitions().filter(t => LOCAL_CORE_TOOLS.has(t.function.name))
    : openAICompatToolDefinitions()

  // Append MCP tools as Anthropic-format and OpenAI-compat-format entries
  const anthropicMcp = mcpTools  // already Anthropic format from getMcpToolDefinitions
  const openaiMcp = mcpTools.map(t => ({
    type: 'function',
    function: { name: t.name, description: t.description, parameters: t.input_schema },
  }))

  return {
    anthropic: [...anthropicBase, ...anthropicMcp],
    openai: [...openaiBase, ...openaiMcp],
  }
}

const ollamaCapabilityCache = new Map()

async function detectOllamaToolSupport(baseUrl, modelName, signal) {
  const key = `${baseUrl}::${modelName}`
  if (ollamaCapabilityCache.has(key)) return ollamaCapabilityCache.get(key)

  let result = true
  try {
    const apiBase = baseUrl.replace(/\/v1\/?$/, '').replace(/\/+$/, '')
    // #22: compose rt.signal with the 3 s timeout so Esc cancels the probe immediately
    const timeoutSignal = AbortSignal.timeout(3000)
    const fetchSignal = signal ? AbortSignal.any([signal, timeoutSignal]) : timeoutSignal
    const resp = await fetch(`${apiBase}/api/show`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ name: modelName }),
      signal: fetchSignal,
    })
    if (!resp.ok) { result = true }
    else {
      const data = await resp.json()
      result = !!(
        data.template?.includes('.Tools') ||
        data.modelfile?.includes('.Tools') ||
        data.capabilities?.includes('tools')
      )
    }
  } catch (e) {
    // #22: re-throw user aborts — don't swallow as "assume native"
    if (e?.name === 'AbortError') throw e
    result = true
  }

  ollamaCapabilityCache.set(key, result)
  return result
}

function mapAnthropicContent(content) {
  return content.map(b => {
    if (b.type === 'text') return { type: 'text', text: b.text }
    if (b.type === 'tool_use') return { type: 'tool_use', id: b.id, name: b.name, input: b.input || {} }
    return null
  }).filter(Boolean)
}

function classifyError(e) {
  const msg = String(e?.message || e).toLowerCase()
  if (msg.includes('401')) return 'authentication_failed'
  if (msg.includes('429')) return 'rate_limit'
  if (msg.includes('404')) return 'model_not_found'
  if (msg.includes('500') || msg.includes('503')) return 'server_error'
  return 'unknown'
}

/** Re-throw user aborts so Esc is not swallowed as a soft model failure (issue #7). */
function rethrowIfAborted(e) {
  if (!isAbortError(e)) return
  if (e.name === 'AbortError') throw e
  const err = new Error('abort')
  err.name = 'AbortError'
  err.cause = e
  throw err
}

async function askHuman(io, q) {
  if (!process.stdin.isTTY) return true
  if (io?.ask) { const ans = await io.ask(q); return ans != null && ans.trim().toLowerCase().startsWith('y') }
  const rl = readline.createInterface({ input: process.stdin, output: process.stdout })
  return new Promise(resolve => {
    rl.question(q, ans => { rl.close(); resolve(/^y(es)?$/i.test(String(ans || '').trim())) })
  })
}

function parseManualToolCalls(text) {
  const calls = []
  const ticks = String.fromCharCode(96, 96, 96)
  const pattern = ticks + '(?:json)?\\s*(\\{[\\s\\S]*?\\})\\s*' + ticks
  const blockRegex = new RegExp(pattern, 'g')
  let match
  while ((match = blockRegex.exec(text)) !== null) {
    try {
      const parsed = JSON.parse(match[1].trim())
      if (parsed.tool) {
        calls.push({ type: 'tool_use', id: `manual-${Math.random().toString(36).slice(2, 9)}`, name: parsed.tool, input: parsed.input || {} })
      }
    } catch { }
  }
  return calls
}

function parseXmlToolCalls(text, knownToolNames = null) {
  if (!text || !text.includes('[TOOL_CALL]')) return []
  const calls = []

  let cursor = 0
  while (true) {
    const startIdx = text.indexOf('[TOOL_CALL]', cursor)
    if (startIdx === -1) break

    const contentStart = startIdx + '[TOOL_CALL]'.length
    let endIdx = text.indexOf('[/TOOL_CALL]', contentStart)
    if (endIdx === -1) {
      endIdx = text.length
    }

    cursor = endIdx + (endIdx === text.length ? 0 : '[/TOOL_CALL]'.length)

    let raw = text.slice(contentStart, endIdx).trim()
    if (!raw) continue

    raw = raw.replace(/<\/?arguments>/g, '').replace(/<\/?tool_call>/g, '').trim()

    const firstBrace = raw.indexOf('{')
    const lastBrace = raw.lastIndexOf('}')

    if (firstBrace === -1) continue

    if (lastBrace > firstBrace) {
      raw = raw.slice(firstBrace, lastBrace + 1)
    } else {
      raw = raw.slice(firstBrace)
    }

    // Repair the `{"ToolName", "arguments": {...}}` shorthand — model drops
    // the "name": key and leaves the tool name as a bare leading string.
    raw = raw.replace(/^\{\s*"([A-Za-z_][\w-]*)"\s*,/, '{"name":"$1",')

    let parsed = null
    try {
      parsed = JSON.parse(raw)
    } catch {
      let openBraces = 0, openBrackets = 0
      for (const ch of raw) {
        if (ch === '{') openBraces++
        else if (ch === '}') openBraces--
        else if (ch === '[') openBrackets++
        else if (ch === ']') openBrackets--
      }

      let repaired = raw
      while (openBrackets > 0) { repaired += ']'; openBrackets-- }
      while (openBraces > 0) { repaired += '}'; openBraces-- }

      try {
        parsed = JSON.parse(repaired)
      } catch {
        continue
      }
    }

    if (!parsed || typeof parsed !== 'object') continue

    const name = parsed.name || parsed.tool
    if (!name || typeof name !== 'string') continue
    if (knownToolNames && knownToolNames.size > 0 && !knownToolNames.has(name)) continue

    const args = parsed.arguments || parsed.input
    const input = (args && typeof args === 'object' && !Array.isArray(args)) ? args : {}

    calls.push({
      type: 'tool_use',
      id: `tc-${Math.random().toString(36).slice(2, 9)}`,
      name,
      input,
    })
  }

  return calls
}

function sanitizeModelText(text) {
  if (!text) return text
  return text
    .replace(/<[｜|]begin[▁_]of[▁_]sentence[｜|]>/g, '')
    .replace(/<[｜|]end[▁_]of[▁_]sentence[｜|]>/g, '')
    .replace(/<\|im_start\|>\w*/g, '')
    .replace(/<\|im_end\|>/g, '')
    .trim()
}

function sanitizeMessages(messages) {
  return messages.map(m => {
    if (typeof m.content === 'string') {
      return { ...m, content: sanitizeModelText(m.content) }
    }
    if (Array.isArray(m.content)) {
      return {
        ...m,
        content: m.content.map(block => {
          if (typeof block === 'string') return sanitizeModelText(block)
          if (block?.text) return { ...block, text: sanitizeModelText(block.text) }
          if (block?.content && typeof block.content === 'string') return { ...block, content: sanitizeModelText(block.content) }
          return block
        }),
      }
    }
    return m
  })
}

const CONTENT_CAP_CHARS = 6000
const HEADER_LINES = 50

function extractFilesRead(db, sessionId) {
  const rows = db.prepare(`
    SELECT entry_type, payload_json FROM transcript_entries
    WHERE session_id = ? ORDER BY sequence ASC
  `).all(sessionId)

  const toolCalls = new Map()
  for (const row of rows) {
    try {
      const payload = JSON.parse(row.payload_json)
      if (row.entry_type === 'assistant') {
        for (const b of payload.content || []) {
          if (b.type === 'tool_use' && b.name === 'Read') {
            toolCalls.set(b.id, b.input?.path || b.input?.file_path || '(unknown)')
          }
        }
      }
    } catch { }
  }

  const seen = new Set()
  const files = []

  for (const row of rows) {
    try {
      const payload = JSON.parse(row.payload_json)
      if (row.entry_type === 'tool_result') {
        const filePath = toolCalls.get(payload.tool_use_id)
        if (!filePath || payload.is_error || seen.has(filePath)) continue
        seen.add(filePath)

        const raw = typeof payload.content === 'string'
          ? payload.content
          : (payload.content?.[0]?.text ?? '')

        const content = raw.length > CONTENT_CAP_CHARS
          ? raw.slice(0, CONTENT_CAP_CHARS) + `\n... [truncated — ${raw.length} chars total]`
          : raw

        const header = raw.split('\n').slice(0, HEADER_LINES).join('\n')

        files.push({ path: filePath, content, header })
      }
    } catch { }
  }

  return files
}

async function summarizeFilesAnthropicForTask(client, rt, filesRead, userRequest, io) {
  if (!filesRead.length) return []
  const summaries = []

  for (const { path, content, header } of filesRead) {
    if (io.spinner) io.spinner.start(`Summarising ${path.split('/').pop()}`)
    const prompt = `File path: ${path}

File content:
\`\`\`
${content}
\`\`\`

Task: ${userRequest}

In 4-6 sentences, what does this file tell you that is directly relevant to accomplishing the task?
Focus on: exact script names, function signatures, argument names, expected file paths, output locations, prerequisites, and any constraints.
If this file is not relevant to the task, say "Not relevant" and nothing else.`

    try {
      const response = await client.messages.create({
        model: rt._model,
        max_tokens: 512,
        messages: [{ role: 'user', content: prompt }],
      }, { signal: rt.signal })
      const summary = sanitizeModelText(response.content?.[0]?.text || '')
      trackTokens(rt.sessionId, response.usage?.input_tokens, response.usage?.output_tokens)
      if (summary && !/^not relevant/i.test(summary)) {
        summaries.push({ path, header, summary })
      }
    } catch (e) {
      rethrowIfAborted(e)
      summaries.push({ path, header, summary: '(summarization failed — see header)' })
    }
    if (io.spinner) io.spinner.stop()
  }

  return summaries
}

async function summarizeFilesOpenAICompatForTask(baseUrl, apiKey, rt, filesRead, userRequest, io) {
  if (!filesRead.length) return []
  const summaries = []

  for (const { path, content, header } of filesRead) {
    if (io.spinner) io.spinner.start(`Summarising ${path.split('/').pop()}`)
    const prompt = `File path: ${path}

File content:
\`\`\`
${content}
\`\`\`

Task: ${userRequest}

In 4-6 sentences, what does this file tell you that is directly relevant to accomplishing the task?
Focus on: exact script names, function signatures, argument names, expected file paths, output locations, prerequisites, and any constraints.
If this file is not relevant to the task, say "Not relevant" and nothing else.`

    try {
      const out = await streamOpenAIChatCompletion({
        baseUrl, apiKey,
        model: rt._model,
        messages: [{ role: 'user', content: prompt }],
        tools: [],
        io,
        bufferOutput: true,
        maxTokens: 512,
        numCtx: rt.settings?.model_config?.num_ctx,
        signal: rt.signal,
      })
      const summary = sanitizeModelText(out.bufferedText || '')
      if (out.usage) trackTokens(rt.sessionId, out.usage.prompt_tokens, out.usage.completion_tokens)
      if (summary && !/^not relevant/i.test(summary)) {
        summaries.push({ path, header, summary })
      }
    } catch (e) {
      rethrowIfAborted(e)
      summaries.push({ path, header, summary: '(summarization failed — see header)' })
    }
    if (io.spinner) io.spinner.stop()
  }

  return summaries
}

async function exploreGateAnthropic(db, rt, io, client, userRequest) {
  const filesRead = extractFilesRead(db, rt.sessionId)
  if (!filesRead.length) {
    try {
      await ingestObservation(db, {
        sessionId: rt.sessionId,
        conversationId: rt.conversationId,
        title: 'Exploration exhausted',
        text: 'No matching context in current repo; provide clarification or expanded scope.',
        provenance: 'exploration_exhausted',
        filePaths: [],
        layaOpts: rt.layaOpts || {},
      })
    } catch (err) {
      // Non-fatal: exploration handoff must not abort on JEV ingest failure.
      try { console.error(`[orchestrate] JEV ingest skipped: ${err && err.message}`) } catch {}
    }
  }
  io.println(`✓ Exploration complete. Summarising ${filesRead.length} file(s)...`)

  const summaries = await summarizeFilesAnthropicForTask(client, rt, filesRead, userRequest, io)
  // #11: persist summaries so a failed planning turn can be retried without re-exploration.
  upsertSummary(db, rt.conversationId, JSON.stringify(summaries))
  appendEntry(db, rt.sessionId, 'user', makeUserPayload(buildDiagnoseUserMessage(summaries, userRequest)))
  const exploreDigest = filesRead.map((file) => (file && file.path) || '').filter(Boolean).join(' → ') || 'none'
  upsertPinnedContext(db, rt.sessionId, 'explore_candidates_snapshot', exploreDigest)
  withTransaction(db, () => {
    db.prepare(`INSERT INTO events (conversation_id, event_type, detail) VALUES (?, 'exploration_complete', ?)`).run(
      rt.conversationId,
      JSON.stringify({ basis: 'explore_gate', files: filesRead.map((file) => (file && file.path) || '').filter(Boolean) })
    )
  })
  setPhase(db, rt.conversationId, 'diagnose', 'explore_complete')
  io.println('Entering diagnose phase.')
  return true
}

export async function exploreGateOpenAICompat(db, rt, io, provider, baseUrl, apiKey, userRequest) {
  const filesRead = extractFilesRead(db, rt.sessionId)
  if (!filesRead.length) {
    try {
      await ingestObservation(db, {
        sessionId: rt.sessionId,
        conversationId: rt.conversationId,
        title: 'Exploration exhausted',
        text: 'No matching context in current repo; provide clarification or expanded scope.',
        provenance: 'exploration_exhausted',
        filePaths: [],
        layaOpts: rt.layaOpts || {},
      })
    } catch (err) {
      // Non-fatal: exploration handoff must not abort on JEV ingest failure.
      try { console.error(`[orchestrate] JEV ingest skipped: ${err && err.message}`) } catch {}
    }
  }
  io.println(`✓ Exploration complete. Summarising ${filesRead.length} file(s)...`)

  const summaries = await summarizeFilesOpenAICompatForTask(baseUrl, apiKey, rt, filesRead, userRequest, io)
  // #11: persist summaries so a failed planning turn can be retried without re-exploration.
  upsertSummary(db, rt.conversationId, JSON.stringify(summaries))
  appendEntry(db, rt.sessionId, 'user', makeUserPayload(buildDiagnoseUserMessage(summaries, userRequest)))
  const exploreDigest = filesRead.map((file) => (file && file.path) || '').filter(Boolean).join(' → ') || 'none'
  upsertPinnedContext(db, rt.sessionId, 'explore_candidates_snapshot', exploreDigest)
  withTransaction(db, () => {
    db.prepare(`INSERT INTO events (conversation_id, event_type, detail) VALUES (?, 'exploration_complete', ?)`).run(
      rt.conversationId,
      JSON.stringify({ basis: 'explore_gate', files: filesRead.map((file) => (file && file.path) || '').filter(Boolean) })
    )
  })
  setPhase(db, rt.conversationId, 'diagnose', 'explore_complete')
  io.println('Entering diagnose phase.')
  return true
}

export async function diagnoseGate(db, rt, io, text) {
  withTransaction(db, () => {
    db.prepare(`INSERT INTO events (conversation_id, event_type, detail) VALUES (?, 'diagnosis_recorded', ?)`).run(
      rt.conversationId,
      JSON.stringify({ diagnosis: String(text || rt?._userText || 'diagnosis').slice(0, 400) })
    )
  })
  setPhase(db, rt.conversationId, 'create_issue', 'diagnosis_recorded')
  return true
}

export async function createIssueGate(db, rt, io, text) {
  const titleSource = String(text || rt?._userText || 'SDLC issue').trim().split(/\n/)[0].replace(/^#+\s*/, '')
  const title = (titleSource || 'SDLC workflow issue').slice(0, 120)
  // Carry the latest actually recorded diagnosis for this conversation into the
  // real issue artifact instead of substituting generic filler text.
  let diagnosis = null
  try {
    const row = db.prepare(
      `SELECT detail FROM events WHERE conversation_id = ? AND event_type = 'diagnosis_recorded' ORDER BY id DESC LIMIT 1`
    ).get(rt.conversationId)
    if (row && row.detail) {
      const parsed = JSON.parse(row.detail)
      if (parsed && typeof parsed === 'object' && !Array.isArray(parsed) && typeof parsed.diagnosis === 'string' && parsed.diagnosis.trim()) {
        diagnosis = parsed.diagnosis.trim()
      }
    }
  } catch {}
  const resolvedDiagnosis = diagnosis || String(text || rt?._userText || '').trim() || 'No diagnosis recorded for this conversation.'
  const issue = createIssue(rt.cwd, {
    title,
    observed: String(text || rt?._userText || title),
    expected: `Resolve ${title} and satisfy the approved acceptance criteria.`,
    diagnosis: resolvedDiagnosis,
    acceptanceCriteria: [
      'Issue file is created under .issues',
      'issue_created event is persisted for the conversation',
      'create_tests becomes the accepted next phase',
    ],
  })
  withTransaction(db, () => {
    db.prepare(`INSERT INTO events (conversation_id, event_type, detail) VALUES (?, 'issue_created', ?)`).run(
      rt.conversationId,
      JSON.stringify({ issueNumber: issue.number, file: issue.file })
    )
  })
  setPhase(db, rt.conversationId, 'create_tests', 'issue_created')
  io && typeof io.println === 'function' && io.println(`✓ Issue recorded: ${issue.file}`)
  return true
}

export async function createTestsGate(db, rt, io, text) {
  let redEvidence = getOriginalRedEvidence(db, rt.conversationId)
  // A tool execution may have already persisted a genuine failing run (source
  // "tool_boundary"). Durably associate that real evidence before planning.
  if (redEvidence && Number(redEvidence.exit_code) !== 0 &&
      String(redEvidence.source || 'manual') === 'tool_boundary') {
    recordRedTestEvidence(db, rt.conversationId, {
      command: redEvidence.command,
      exit_code: redEvidence.exit_code,
      output_digest: redEvidence.output_digest,
      source: 'manual',execution:redEvidence.execution,
    })
    redEvidence = getOriginalRedEvidence(db, rt.conversationId)
  }
  if (!redEvidence || Number(redEvidence.exit_code) === 0) {
    io && typeof io.println === 'function' && io.println('✓ RED test evidence is required before planning.')
    return false
  }
  if(!setPhase(db, rt.conversationId, 'planning', 'red_tests_confirmed'))return false
  const summaries=db.prepare('SELECT content FROM summaries WHERE conversation_id=?').get(rt.conversationId)?.content
  let files=[];try{files=JSON.parse(summaries||'[]')}catch{}
  const recordedIssue=db.prepare("SELECT detail FROM events WHERE conversation_id=? AND event_type='issue_created' ORDER BY id DESC LIMIT 1").get(rt.conversationId)?.detail||''
  appendEntry(db,rt.sessionId,'user',makeUserPayload(buildPlanningUserMessage(files,rt._userText||'')+'\nRecorded issue: '+recordedIssue+'\nOriginal RED execution: '+redEvidence.command+'\n'+redEvidence.output_digest))
  io && typeof io.println === 'function' && io.println('✓ RED confirmed. Entering planning phase.')
  return true
}

export async function verifyGate(db, rt, io, text) {
  const hasGreen=getCurrentGreenEvidence(db,rt.conversationId)
  if (!hasGreen) {
    io && typeof io.println === 'function' && io.println('[verify] GREEN test result is required before safety phase.')
    return false
  }
  setPhase(db, rt.conversationId, 'safety', 'verify_green')
  return true
}

export async function safetyGate(db, rt, io, text) {
  const result = await runSafetyChecks(db, rt, io)
  const checkerRows = db.prepare(`SELECT detail FROM events WHERE conversation_id = ? AND event_type = 'safety_checker'`).all(rt.conversationId)
  const passed = new Set()
  for (const row of checkerRows) {
    try {
      const detail = JSON.parse(row.detail || '{}')
      if (detail.passed === true) passed.add(detail.checkerName || detail.checker || detail.checker_id)
    } catch {}
  }
  let coverage = 0
  const branchCoverage = collectBranchCoverage(db, rt)
  if (branchCoverage.accepted) {
    coverage = Number(branchCoverage.ratio) * 100
  }
  if (!Number.isFinite(coverage) || coverage < 70) {
    withTransaction(db, () => {
      db.prepare(`INSERT INTO events (conversation_id, event_type, detail) VALUES (?, 'coverage_evidence_missing', ?)`).run(
        rt.conversationId,
        JSON.stringify({ measured: false, blocking: true, threshold: 70, observed: Number.isFinite(coverage) ? coverage : null, reason: 'qualifying measured path coverage evidence absent' })
      )
    })
    coverage = 0
  }
  if (result.passed && hasCurrentSafetyChecks(db,rt.conversationId) && coverage >= 70) {
    const advanced = setPhase(db, rt.conversationId, 'confirm_fix', 'safety_complete')
    if (!advanced) return false
    io && typeof io.println === 'function' && io.println('✓ Safety checks passed. Entering confirm_fix phase.')
    return true
  }
  return false
}

export async function confirmFixGate(db, rt, io, text) {
  const answer = io && io.ask
    ? await io.ask('Confirm fix and allow commit? [y/N] ')
    : 'n'
  if (!/^y(es)?$/i.test(String(answer || '').trim())) {
    try {
      withTransaction(db, () => {
        db.prepare(`INSERT INTO events (conversation_id, session_id, event_type, detail) VALUES (?, ?, 'fix_rejected', ?)`).run(
          rt.conversationId,
          rt.sessionId || null,
          JSON.stringify({ approved: false, refused: true, operatorAnswer: String(answer || '').trim(), reason: 'operator declined fix' })
        )
      })
    } catch (err) {
      try { io && typeof io.println === 'function' && io.println(`[confirm_fix] Refusal event could not be persisted: ${err && err.message ? err.message : err}`) } catch {}
    }
    io && typeof io.println === 'function' && io.println('Confirmation rejected. Remaining in confirm_fix phase.')
    return false
  }
  withTransaction(db, () => {
    db.prepare(`INSERT INTO events (conversation_id, event_type, detail) VALUES (?, 'fix_confirmed', ?)`).run(
      rt.conversationId,
      JSON.stringify({ approved: true })
    )
  })
  if(!setPhase(db, rt.conversationId, 'done', 'fix_confirmed'))return false
  io && typeof io.println === 'function' && io.println('✓ Fix confirmed. Entering done phase.')
  return true
}


function planIssueReferenceMatches(db, rt, planText) {
  const clean = sanitizeModelText(planText)
  try {
    const issueRow = db.prepare(
      `SELECT detail FROM events WHERE conversation_id = ? AND event_type = 'issue_created' ORDER BY id DESC LIMIT 1`
    ).get(rt.conversationId)
    if (!issueRow || !issueRow.detail) return true
    const parsed = JSON.parse(issueRow.detail)
    if (!parsed || typeof parsed !== 'object' || Array.isArray(parsed) || !Number.isInteger(parsed.issueNumber) || parsed.issueNumber <= 0) return true
    const actualIssueNumber = parsed.issueNumber
    const allowedNumbers = new Set([actualIssueNumber])
    try {
      if (parsed.file && typeof parsed.file === 'string' && fs.existsSync(parsed.file)) {
        const issueContent = fs.readFileSync(parsed.file, 'utf8')
        for (const m of issueContent.matchAll(/#(\d+)/g)) allowedNumbers.add(Number(m[1]))
      }
    } catch {}
    const issueSectionMatch = clean.match(/##\s*Issue Reference\b([\s\S]*?)(?=##\s|$)/i)
    const searchSpace = issueSectionMatch ? issueSectionMatch[1] : clean
    const referenced = [...searchSpace.matchAll(/#(\d+)/g)].map((m) => Number(m[1]))
    if (referenced.length === 0) return false
    if (referenced.some((n) => !allowedNumbers.has(n))) return false
    return true
  } catch {
    return true
  }
}

export async function planGate(db, rt, io, planText) {
  const clean = sanitizeModelText(planText)
  if (!clean) {
    io.println('\n[planning] Model produced no plan text — response empty after reasoning-strip.')
    return false
  }

  // AGENTS.md Phase 4: require pre-recorded failing (RED) behavior before plan approval.
  const redEvidence = getOriginalRedEvidence(db, rt.conversationId)
  if (!redEvidence || Number(redEvidence.exit_code) === 0) {
    io.println('\n[planning] Plan rejected: RED test evidence is required before planning approval.')
    return false
  }

  // AGENTS.md Phase 5/6: reject plans that lack substantive issue reference,
  // Given/When/Then behavioral specification, or a coverage matrix with a real scenario row.
  const issueSection = (clean.match(/##\s*Issue Reference\b([\s\S]*?)(?=##\s|$)/i) || [null, ''])[1] || ''
  const behavioralSection = (clean.match(/##\s*Behavioral Specification\b([\s\S]*?)(?=##\s|$)/i) || [null, ''])[1] || ''
  const coverageSection = (clean.match(/##\s*Coverage Matrix\b([\s\S]*?)(?=##\s|$)/i) || [null, ''])[1] || ''
  const hasIssueRef = /#\d+/.test(issueSection)
  const hasBehavioralSpec = /given[\s\S]{0,200}when[\s\S]{0,200}then/i.test(behavioralSection)
  const hasCoverageMatrix = !!coverageSection.trim()
  const coverageScenarioCount = countPlanScenarios(clean)
  if (!hasIssueRef) {
    io.println('\n[planning] Plan rejected: missing issue reference (e.g. #1) required by AGENTS.md Phase 3.')
    io.println('[planning] Create a local issue first (/.issues/) and reference it in the plan.')
    return false
  }
  if (!hasBehavioralSpec) {
    io.println('\n[planning] Plan rejected: missing Behavioral Specification (Given-When-Then) required by AGENTS.md Phase 5.')
    io.println('[planning] Add a Given-When-Then section or table before requesting operator approval.')
    return false
  }
  if (!hasCoverageMatrix || coverageScenarioCount < 1) {
    io.println('\n[planning] Plan rejected: missing Coverage Matrix with at least one mapped scenario required by AGENTS.md Phase 5.')
    io.println('[planning] Add a "## Coverage Matrix" table mapping each scenario to its test file before requesting operator approval.')
    return false
  }

  // Bind the plan to the actual issue_created artifact for this conversation when one exists.
  // Reject wrong (but syntactically valid) issue references before any operator HITL prompt.
  let actualIssueNumber = null
  try {
    const issueRow = db.prepare(
      `SELECT detail FROM events WHERE conversation_id = ? AND event_type = 'issue_created' ORDER BY id DESC LIMIT 1`
    ).get(rt.conversationId)
    if (issueRow && issueRow.detail) {
      const parsed = JSON.parse(issueRow.detail)
      if (parsed && typeof parsed === 'object' && !Array.isArray(parsed) && Number.isInteger(parsed.issueNumber) && parsed.issueNumber > 0) {
        actualIssueNumber = parsed.issueNumber
      }
    }
  } catch {}
  if (actualIssueNumber !== null) {
    const allowedNumbers = new Set([actualIssueNumber])
    try {
      const issueRow2 = db.prepare(`SELECT detail FROM events WHERE conversation_id = ? AND event_type = 'issue_created' ORDER BY id DESC LIMIT 1`).get(rt.conversationId)
      if (issueRow2?.detail) {
        const parsed2 = JSON.parse(issueRow2.detail)
        if (parsed2?.file && typeof parsed2.file === 'string' && fs.existsSync(parsed2.file)) {
          const issueContent = fs.readFileSync(parsed2.file, 'utf8')
          for (const m of issueContent.matchAll(/#(\d+)/g)) allowedNumbers.add(Number(m[1]))
        }
      }
    } catch {}
    const issueSectionMatch = clean.match(/##\s*Issue Reference\b([\s\S]*?)(?=##\s|$)/i)
    const searchSpace = issueSectionMatch ? issueSectionMatch[1] : clean
    const referenced = [...searchSpace.matchAll(/#(\d+)/g)].map((m) => Number(m[1]))
    if (referenced.length === 0 || referenced.some((n) => !allowedNumbers.has(n))) {
      io.println('\n[planning] Plan rejected: issue reference must match the recorded issue for this conversation.')
      io.println(`[planning] Expected issue #${actualIssueNumber}.`)
      return false
    }
  }

  io.println(`\n${'─'.repeat(60)}\n${clean}\n${'─'.repeat(60)}`)
  const answer = io.ask ? await io.ask('Approve this plan? [y/N] ') : 'n'
  if (!/^y(es)?$/i.test(String(answer || '').trim())) {
    io.println('Plan rejected. Revise your request or try again.')
    try {
      withTransaction(db, () => {
        db.prepare(`INSERT INTO events (conversation_id, session_id, event_type, detail) VALUES (?, ?, 'plan_rejected', ?)`).run(
          rt.conversationId,
          rt.sessionId || null,
          JSON.stringify({ approved: false, refused: true, operatorAnswer: String(answer || '').trim(), reason: 'operator declined plan' })
        )
      })
    } catch (err) {
      try { io.println(`[planning] Plan rejection event could not be persisted: ${err && err.message ? err.message : err}`) } catch {}
    }
    return false
  }
  const hash = randomUUID().slice(0, 16)
  withTransaction(db, () => {
    db.prepare(`INSERT INTO plans(conversation_id, content, hash, status, approved_at) VALUES (?,?,?,'approved',datetime('now'))`).run(rt.conversationId, clean, hash)
  })
  if(!setPhase(db, rt.conversationId, 'implement', 'plan_approved'))return false
  io.println('✓ Plan approved. Entering implementation phase.')
  return true
}

/**
 * #11 — retry the planning turn exactly once when the model produced no plan text
 * (think-only / empty response). The first attempt's raw response and a nudge user
 * message are persisted to the transcript so the failure stays diagnosable.
 * Returns true only if the retried plan is approved.
 */
export async function retryEmptyPlanningTurn(db, rt, io, runPlanPhase) {
  io.println('\n[planning] Empty plan response — retrying once with a nudge...')
  appendEntry(db, rt.sessionId, 'user', makeUserPayload(
    'Your previous response contained no plan text (it was empty after reasoning-trace stripping). ' +
    'Output the plan as prose only. Do NOT use any tools. Do NOT include <think1> blocks or reasoning. ' +
    'Start your reply with "## Objective".'
  ))
  const text = await runPlanPhase()
  if (!text || !String(text).trim()) {
    io.println('\n[planning] No plan text after retry — aborting planning phase. Transcript retained for diagnosis.')
    return false
  }
  return planGate(db, rt, io, text)
}

async function implementGate(db, rt, io, summaryText) {
  if (summaryText) io.println(`\n${'─'.repeat(60)}\n${summaryText}\n${'─'.repeat(60)}`)
  const answer = io.ask ? await io.ask('Accept implementation and run tests? [y/N] ') : 'n'
  if (!/^y(es)?$/i.test(String(answer || '').trim())) {
    io.println('Implementation rejected. Continuing in implement phase.')
    return false
  }
  withTransaction(db, () => {
    db.prepare(`INSERT INTO events (conversation_id, event_type, detail) VALUES (?, 'implement', ?)`).run(
      rt.conversationId,
      JSON.stringify({ accepted: true })
    )
  })
  const ok = setPhase(db, rt.conversationId, 'verify', 'implementation_accepted')
  if (!ok) {
    io.println('Implementation acceptance did not advance to verify.')
    return false
  }
  io.println('✓ Implementation accepted. Entering verify phase.')
  return true
}

async function testGate(db, rt, io, summaryText) {
  if (summaryText) io.println(`\n${'─'.repeat(60)}\n${summaryText}\n${'─'.repeat(60)}`)
  const answer = io.ask ? await io.ask('Accept test results and complete task? [y/N] ') : 'n'
  if (!/^y(es)?$/i.test(String(answer || '').trim())) {
    io.println('Test results rejected. Continuing in test phase.')
    return false
  }
  setPhase(db, rt.conversationId, 'idle', 'tests_accepted')
  io.println('✓ Task complete. Returning to idle.')
  return true
}

function extractRecentToolResults(db, sessionId) {
  const rows = db.prepare(`
    SELECT entry_type, payload_json FROM transcript_entries
    WHERE session_id = ? ORDER BY sequence ASC
  `).all(sessionId)

  let lastUserIdx = -1
  for (let i = rows.length - 1; i >= 0; i--) {
    if (rows[i].entry_type === 'user') { lastUserIdx = i; break }
  }

  const startIdx = lastUserIdx >= 0 ? lastUserIdx + 1 : 0
  const toolCalls = new Map()
  const results = []

  for (let i = startIdx; i < rows.length; i++) {
    const row = rows[i]
    try {
      const payload = JSON.parse(row.payload_json)
      if (row.entry_type === 'assistant') {
        for (const b of payload.content || []) {
          if (b.type === 'tool_use') toolCalls.set(b.id, b.name)
        }
      } else if (row.entry_type === 'tool_result') {
        const name = toolCalls.get(payload.tool_use_id) || 'tool'
        const content = typeof payload.content === 'string' ? payload.content : JSON.stringify(payload.content)
        const status = payload.is_error ? '✗' : '✓'
        results.push(`${status} ${name}: ${content.slice(0, 300)}`)
      }
    } catch { }
  }

  if (!results.length) {
    for (const row of rows) {
      try {
        const payload = JSON.parse(row.payload_json)
        if (row.entry_type === 'assistant') {
          for (const b of payload.content || []) {
            if (b.type === 'tool_use') toolCalls.set(b.id, b.name)
          }
        } else if (row.entry_type === 'tool_result') {
          const name = toolCalls.get(payload.tool_use_id) || 'tool'
          const content = typeof payload.content === 'string' ? payload.content : JSON.stringify(payload.content)
          const status = payload.is_error ? '✗' : '✓'
          if (!results.some(r => r.includes(content.slice(0, 50)))) {
            results.push(`${status} ${name}: ${content.slice(0, 300)}`)
          }
        }
      } catch { }
    }
  }

  return results.length ? results.join('\n') : '(no tool results recorded)'
}

async function runAnthropicSummaryTurn(db, rt, io, hookRt, client, summaryPrompt) {
  appendEntry(db, rt.sessionId, 'user', makeUserPayload(summaryPrompt))
  if (io.spinner) io.spinner.start('Summarising')

  const currentPhase = getPhase(db, rt.conversationId)
  const system = buildSystemPrompt(rt.cwd, activeProvider(rt.settings), rt._model, currentPhase, rt.onaInstructions, null, rt._intentMeta)
  const messages = applyEpistemicFilter(transcriptToAnthropicMessages(db, rt.sessionId), currentPhase)

  let bufferedText = ''
  try {
    const stream = await client.messages.stream(
      { model: rt._model, max_tokens: 1024, system, messages, tools: [] },
      { signal: rt.signal },
    )
    for await (const ev of stream) {
      if (ev.type === 'content_block_delta' && ev.delta?.type === 'text_delta' && ev.delta.text) {
        bufferedText += ev.delta.text
      }
    }
    const final = await stream.finalMessage()
    trackTokens(rt.sessionId, final.usage?.input_tokens, final.usage?.output_tokens)
    appendEntry(db, rt.sessionId, 'assistant', makeAssistantPayload(mapAnthropicContent(final.content || [])))
  } catch (e) {
    rethrowIfAborted(e)
    if (io.spinner) io.spinner.stop()
    return null
  }
  if (io.spinner) io.spinner.stop()
  return sanitizeModelText(bufferedText)
}

async function runOpenAICompatSummaryTurn(db, rt, io, hookRt, provider, baseUrl, apiKey, summaryPrompt) {
  appendEntry(db, rt.sessionId, 'user', makeUserPayload(summaryPrompt))
  if (io.spinner) io.spinner.start('Summarising')

  const currentPhase = getPhase(db, rt.conversationId)
  const systemPrompt = buildSystemPrompt(rt.cwd, provider, rt._model, currentPhase, rt.onaInstructions, null, rt._intentMeta)
  const rawMessages = transcriptToOpenAIMessages(db, rt.sessionId)
  const messages = [{ role: 'system', content: systemPrompt }, ...sanitizeMessages(rawMessages)]

  let result = null
  try {
    const out = await streamOpenAIChatCompletion({ baseUrl, apiKey, model: rt._model, messages, tools: [], io, bufferOutput: true, numCtx: rt.settings?.model_config?.num_ctx, signal: rt.signal })
    if (out.usage) trackTokens(rt.sessionId, out.usage.prompt_tokens, out.usage.completion_tokens)
    appendEntry(db, rt.sessionId, 'assistant', makeAssistantPayload(out.assistantBlocks))
    result = sanitizeModelText(out.bufferedText)
  } catch (e) {
    rethrowIfAborted(e)
  }
  if (io.spinner) io.spinner.stop()
  return result
}

async function runAnthropicPhase(db, rt, io, hookRt, client, tools, phase) {
  for (;;) {
    // #23: Do not reset spinner to 'Thinking' at loop top — it overwrites the
    // lastSpinLabelRef fallback from the previous tool result, causing verb
    // rotation (spitfire). The spinner is already running from the parent
    // function's io.spinner.start('Analyzing') or the lastSpinLabelRef fallback.
    const currentPhase = getPhase(db, rt.conversationId)
    if(currentPhase==='explore'&&rt._exploreStagnant){
      await exploreGateAnthropic(db,rt,io,client,rt._userText)
      rt._exploreHandoffComplete=true
      return 'Exploration stopped after sufficient evidence or a repeated no-progress result.'
    }

    const system = buildSystemPrompt(rt.cwd, activeProvider(rt.settings), rt._model, currentPhase, rt.onaInstructions, null, rt._intentMeta)
    const filteredTools = filterToolsByPhase(tools, currentPhase)

    // Hard exploration request budget: request 20 is allowed, request 21 is blocked.
    // On exhaustion, retain discovered evidence, record the reason, and hand off.
    if (currentPhase === 'explore' && rt._exploreRequestBudgetExhausted !== true) {
      if ((rt._exploreRequestCount || 0) >= 20) {
        const reason = 'explore_request_budget_exhausted'
        io.println(`[explore] Saturation: request budget exhausted at 20 upstream requests (${reason}). Retaining discovered evidence.`)
        withTransaction(db, () => {
          db.prepare(`INSERT INTO events (conversation_id, event_type, detail) VALUES (?, ?, ?)`).run(
            rt.conversationId,
            'saturation',
            JSON.stringify({ reason, limit: 20, blockingRequest: 21 })
          )
          db.prepare(`INSERT INTO events (conversation_id, event_type, detail) VALUES (?, ?, ?)`).run(
            rt.conversationId,
            reason,
            JSON.stringify({ reason: 'saturation', limit: 20, blockingRequest: 21 })
          )
        })
        await exploreGateAnthropic(db, rt, io, client, rt._userText)
        rt._exploreRequestBudgetExhausted = true
        return 'Exploration saturated after reaching the 20-request request budget; discovered evidence retained and phase handed off to diagnose.'
      }
      rt._exploreRequestCount = (rt._exploreRequestCount || 0) + 1
    }

    const rawAnthropic = applyEpistemicFilter(transcriptToAnthropicMessages(db, rt.sessionId), currentPhase)
    const messages = await buildJevSystem2Messages(db, rt, rawAnthropic)

    let assistantBlocks = [], stopReason = null, bufferedText = ''
    try {
      const stream = await client.messages.stream(
        { model: rt._model, max_tokens: 8192, system, messages, tools: filteredTools },
        { signal: rt.signal },
      )
      for await (const ev of stream) {
        if (ev.type === 'content_block_delta' && ev.delta?.type === 'text_delta' && ev.delta.text) {
          bufferedText += ev.delta.text
        }
        if (ev.type === 'content_block_start' && ev.content_block?.type === 'tool_use') {
          if (io.spinner) io.spinner.stop()
          if (io.onToolStart) io.onToolStart(ev.content_block.name)
        }
      }
      const final = await stream.finalMessage()
      stopReason = final.stop_reason
      assistantBlocks = mapAnthropicContent(final.content || [])
      trackTokens(rt.sessionId, final.usage?.input_tokens, final.usage?.output_tokens)
    } catch (e) {
      rethrowIfAborted(e)
      if (io.spinner) io.spinner.stop()
      await runHooks(db, hookRt, 'StopFailure', { error: classifyError(e), error_details: e.message })
      io.println(`\n[model error] ${e.message}`)
      return null
    }

    if (io.spinner) io.spinner.stop()
    appendEntry(db, rt.sessionId, 'assistant', makeAssistantPayload(assistantBlocks))

    const toolUses = assistantBlocks.filter(b => b.type === 'tool_use')

    if (!toolUses.length || stopReason === 'end_turn') {
      return bufferedText
    }

    await executeToolUses(db, rt, io, hookRt, toolUses)
  }
}

export async function runOpenAICompatPhase(db, rt, io, hookRt, provider, baseUrl, apiKey, allTools, injectedTools, phase) {
  let activeTools = injectedTools ? [] : allTools
  let currentInjectedTools = injectedTools

  for (;;) {
    // #23: Do not reset spinner to 'Thinking' at loop top — it overwrites the
    // lastSpinLabelRef fallback from the previous tool result, causing verb
    // rotation (spitfire). The spinner is already running from the parent
    // function's io.spinner.start('Analyzing') or the lastSpinLabelRef fallback.

    const currentPhase = getPhase(db, rt.conversationId)
    if(currentPhase==='explore'&&rt._exploreStagnant){
      await exploreGateOpenAICompat(db,rt,io,provider,baseUrl,apiKey,rt._userText)
      rt._exploreHandoffComplete=true
      return 'Exploration stopped after sufficient evidence or a repeated no-progress result.'
    }

    const systemPrompt = buildSystemPrompt(rt.cwd, provider, rt._model, currentPhase, rt.onaInstructions, currentInjectedTools?filterToolsByPhase(currentInjectedTools,currentPhase):null, rt._intentMeta)
    const filteredTools = filterToolsByPhase(activeTools, currentPhase)
    const xmlInstructions = (omitToolChoice(provider) || provider === 'nehanda')
      ? buildXmlToolInstructions(filteredTools, currentPhase)
      : ''

    // Hard exploration request budget: request 20 is allowed, request 21 is blocked.
    if (currentPhase === 'explore' && rt._exploreRequestBudgetExhausted !== true) {
      if ((rt._exploreRequestCount || 0) >= 20) {
        const reason = 'explore_request_budget_exhausted'
        io.println(`[explore] Saturation: request budget exhausted at 20 upstream requests (${reason}). Retaining discovered evidence.`)
        withTransaction(db, () => {
          db.prepare(`INSERT INTO events (conversation_id, event_type, detail) VALUES (?, ?, ?)`).run(
            rt.conversationId,
            'saturation',
            JSON.stringify({ reason, limit: 20, blockingRequest: 21 })
          )
          db.prepare(`INSERT INTO events (conversation_id, event_type, detail) VALUES (?, ?, ?)`).run(
            rt.conversationId,
            reason,
            JSON.stringify({ reason: 'saturation', limit: 20, blockingRequest: 21 })
          )
        })
        await exploreGateOpenAICompat(db, rt, io, provider, baseUrl, apiKey, rt._userText)
        rt._exploreRequestBudgetExhausted = true
        return 'Exploration saturated after reaching the 20-request request budget; discovered evidence retained and phase handed off to diagnose.'
      }
      rt._exploreRequestCount = (rt._exploreRequestCount || 0) + 1
    }
    const fullSystemPrompt = systemPrompt + xmlInstructions
    const rawMessages = transcriptToOpenAIMessages(db, rt.sessionId)
    const filteredMessages = applyEpistemicFilter(sanitizeMessages(rawMessages), currentPhase)
    const jevMessages = await buildJevSystem2Messages(db, rt, filteredMessages)

    const messages = [{ role: 'system', content: fullSystemPrompt }, ...jevMessages]

    let assistantBlocks, bufferedText = null, finishReason = null
    let out = null
    try {
      // When xmlInstructions is non-empty, tool calling is being driven entirely by
      // prompt-injected [TOOL_CALL] text parsing (parseXmlToolCalls), specifically to
      // route around vLLM's --tool-call-parser qwen3_xml stop-token bug (see README).
      // Sending the native `tools` schema in the same request re-engages vLLM's own
      // --enable-auto-tool-choice machinery (see deploy_sagemaker.sh SM_VLLM_ENABLE_AUTO_TOOL_CHOICE)
      // on top of the prompt-injected instructions, producing malformed hybrid output
      // like {"Glob", "arguments": {...}} instead of {"name": "Glob", "arguments": {...}}.
      const toolsForRequest = xmlInstructions ? [] : filteredTools
      out = await streamOpenAIChatCompletion({ baseUrl, apiKey, model: rt._model, messages, tools: toolsForRequest, io, bufferOutput: true, numCtx: rt.settings?.model_config?.num_ctx, omitToolChoice: omitToolChoice(provider), useStream: !omitToolChoice(provider), signal: rt.signal })
      assistantBlocks = out.assistantBlocks
      bufferedText = out.bufferedText
      finishReason = out.finishReason

      const text = assistantBlocks.filter(b => b.type === 'text').map(b => b.text).join('')
      const knownToolNames = new Set(filteredTools.map(t => t.function?.name).filter(Boolean))
      const xmlCalls = parseXmlToolCalls(text, knownToolNames)
      if (process.env.DEBUG_NEHANDA) console.log('\n[DEBUG] xmlCalls count:', xmlCalls.length, 'knownToolNames:', Array.from(knownToolNames))
      const manualCalls = xmlCalls.length > 0 ? xmlCalls : parseManualToolCalls(text)
      if (manualCalls.length > 0) {
        if (io.spinner) io.spinner.stop()
        for (const call of manualCalls) {
          if (io.onToolStart) io.onToolStart(call.name, call.input || {})
        }
        assistantBlocks = [...assistantBlocks, ...manualCalls]
      }

      if (out.usage) trackTokens(rt.sessionId, out.usage.prompt_tokens, out.usage.completion_tokens)
    } catch (e) {
      rethrowIfAborted(e)
      if (e.message?.includes('does not support tools') && activeTools.length > 0) {
        if (io.spinner) io.spinner.stop()
        io.println('\n[info] Model switching to manual tool injection...')
        activeTools = []
        currentInjectedTools = allTools
        const cacheKey = `${baseUrl}::${rt._model}`
        ollamaCapabilityCache.set(cacheKey, false)
        continue
      }
      if (io.spinner) io.spinner.stop()
      await runHooks(db, hookRt, 'StopFailure', { error: classifyError(e), error_details: e.message })
      io.println(`\n[model error] ${e.message}`)
      return null
    }

    if (io.spinner) io.spinner.stop()

    if (!assistantBlocks.length) {
      // #11: persist a marker entry so an empty/think-only response stays diagnosable
      // instead of vanishing from the transcript before appendEntry ever runs.
      appendEntry(db, rt.sessionId, 'assistant', makeAssistantPayload([{
        type: 'text',
        text: `[empty model response — raw ${out.rawLength ?? 0} chars, finishReason ${finishReason ?? 'unknown'}]`,
      }]))
      return bufferedText
    }

    appendEntry(db, rt.sessionId, 'assistant', makeAssistantPayload(assistantBlocks))
    const toolUses = assistantBlocks.filter(b => b.type === 'tool_use')

    let displayText = bufferedText || ''
    if (displayText) {
      const ticks = String.fromCharCode(96, 96, 96)
      const pattern = ticks + '(?:json)?\\s*\\{[\\s\\S]*?\\}\\s*' + ticks
      displayText = displayText.replace(new RegExp(pattern, 'g'), '').trim()
    }

    if (!toolUses.length) {
      return displayText || bufferedText
    }
    if (finishReason === 'length') {
      return displayText || bufferedText
    }

    await executeToolUses(db, rt, io, hookRt, toolUses)
  }
}

async function executeToolUses(db, rt, io, hookRt, toolUses) {
  for (const tu of toolUses) {
    const toolName = tu.name, toolUseId = tu.id, toolInput = tu.input || {}

    const pre = await runHooks(db, hookRt, 'PreToolUse', { tool_name: toolName, tool_input: toolInput, tool_use_id: toolUseId })
    if (!pre.ok) continue

    const currentPhase = getPhase(db, rt.conversationId)

  // #37 graph-first: an already sufficient JEV graph must prevent exploration from
  // dispatching disk reads entirely. This is distinct from the per-file explore
  // read cache and is evaluated before permission checks, onToolStart, or filesystem access.
  if (currentPhase === 'explore' && toolName === 'Read') {
    if (rt._jevGraphStateUnknown !== false) {
      const query = String(rt?._userText || '').trim()
      const seed = await retrieveEvidence(db, query || 'explore graph sufficiency', {
        sessionId: rt.sessionId,
        conversationId: rt.conversationId,
        layaOpts: rt.layaOpts || { timeoutMs: 50 },
      })
      const evidenceText = (seed.evidence || []).map(e => `- ${e.title}: ${String(e.content || '').slice(0, 300)}`).join('\n')
      const assessment = evidenceText
        ? await assessEvidence(`Query: ${query}\n\nEvidence:\n${evidenceText}`, rt.layaOpts || { timeoutMs: 50 })
        : { sufficient: 0 }
      rt._jevGraphSufficient = seed.stoppedReason === 'sufficient' && (assessment.sufficient || 0) >= 0.9
      rt._jevExploreEvidence = seed
      rt._jevGraphStateUnknown = false
    }
    if (rt._jevGraphSufficient) {
      const evidenceLines = (rt._jevExploreEvidence?.evidence || [])
        .map(e => `• ${e.title}: ${String(e.content || '').slice(0, 120)}`).join('\n')
      const graphResult = [
        'JEV graph evidence is sufficient for this exploration request; filesystem Read bypassed.',
        evidenceLines ? 'Sufficient evidence:\n' + evidenceLines : 'Sufficient evidence retained in JEV graph.',
      ].join('\n')
      const toolResult = makeToolResultPayload(toolUseId, graphResult, false)
      withTransaction(db, () => {
        appendEntry(db, rt.sessionId, 'tool_result', toolResult, toolUseId)
      })
      if (io.onToolResult) io.onToolResult(toolName, graphResult, false)
      rt._exploreStagnant=true
      continue
    }
  }

  const decision = evaluatePermission(rt.settings?.permissions, toolName, toolInput, currentPhase)
    if (decision === 'ask') {
      const ok = await askHuman(io, `Allow ${toolName}? [y/N] `)
      if (!ok) {
        appendEntry(db, rt.sessionId, 'tool_result', makeToolResultPayload(toolUseId, 'User denied permission.', true), toolUseId)
        continue
      }
    } else if (decision === 'deny') {
      appendEntry(db, rt.sessionId, 'tool_result', makeToolResultPayload(toolUseId, 'Permission denied by policy.', true), toolUseId)
      continue
    }

    if (io.onToolStart) io.onToolStart(toolName, toolInput)
    const execCtx = { sessionId: rt.sessionId, conversationId: rt.conversationId, cwd: rt.cwd, settings: rt.settings, signal: rt.signal }
    const out = await executeBuiltinTool(db, execCtx, toolName, toolInput, io)
    rt.signal?.throwIfAborted()
    if(currentPhase==='explore'&&out.exploreDuplicate)rt._exploreStagnant=true

    if (toolName === 'Bash' && out.newCwd) rt.cwd = out.newCwd

    withTransaction(db, () => {
      appendEntry(db, rt.sessionId, 'tool_result', makeToolResultPayload(toolUseId, out.content, out.is_error), toolUseId)
    })

    if (io.onToolResult) io.onToolResult(toolName, out.content, out.is_error)

    await runHooks(db, hookRt, out.is_error ? 'PostToolUseFailure' : 'PostToolUse', {
      tool_name: toolName, tool_input: toolInput, tool_use_id: toolUseId,
      ...(out.is_error ? { error: out.content } : { tool_response: { content: out.content, is_error: out.is_error } }),
    })
  }
}

export async function runUserTurn(db, rt, userText, io) {
  rt._userText = userText
  rt._exploreRequestCount = 0
  rt._exploreRequestBudgetExhausted = false
  rt._exploreStagnant=false
  rt._exploreHandoffComplete=false
  const provider = activeProvider(rt.settings)
  const phase = normalizeLegacyPhase(db, rt.conversationId)
  const hookRt = {
    sessionId: rt.sessionId, conversationId: rt.conversationId,
    runtimeDbPath: rt.runtimeDbPath, cwd: rt.cwd,
    permissionMode: rt.settings?.permissions?.defaultMode ?? 'default',
    settings: rt.settings,
  }

  const ups = await runHooks(db, hookRt, 'UserPromptSubmit', { prompt: userText })
  if (!ups.ok) { io.println(`UserPromptSubmit blocked: ${ups.userPrompt?.stderr || 'exit 2'}`); return }

  // Classify user intent for idle/explore phases so buildSystemPrompt can shape
  // instructions for lower-parameter models. Non-fatal — degraded heuristics cover failure.
  // #24: Run classifyUserIntent concurrently with detectOllamaToolSupport to save
  // up to 3s of sequential wait time. Both are independent operations.
  rt._intentMeta = null

  // Resolve model early so detectOllamaToolSupport can run concurrently
  let model
  try { model = resolveWireModel(rt.settings.model_config) } catch (e) { io.println(`[config] ${e.message}`); return }
  rt._model = model

  // #24: detectOllamaToolSupport is an HTTP probe (up to 3s) independent of
  // classifyUserIntent (Laya call). Run them concurrently.
  const intentPromise = (phase === 'idle' || phase === 'explore')
    ? classifyUserIntent(userText, rt.layaOpts || {}).catch(() => null)
    : Promise.resolve(null)

  const savedBaseUrl = rt.settings?.model_config?.base_url?.replace(/\/+$/, '')
  const ollamaBaseUrl = savedBaseUrl || process.env.OLLAMA_BASE_URL || 'http://localhost:11434/v1'
  const probePromise = (provider === 'ollama')
    ? detectOllamaToolSupport(ollamaBaseUrl, model, rt.signal).catch(() => true)
    : Promise.resolve(true)

  // Await both concurrently
  const [intentResult, nativeSupportResult] = await Promise.all([intentPromise, probePromise])
  rt._intentMeta = intentResult
  rt._ollamaNativeSupport = nativeSupportResult

  if (phase === 'idle' && io.ask) {
    const answer = await io.ask('Use SDLC workflow (explore → diagnose → issue → tests → plan → code → verify → safety)? [Y/n] ')
    const useSdlc = !answer.trim() || /^y(es)?$/i.test(answer.trim())
    if (useSdlc) {
      setPhase(db, rt.conversationId, 'explore', 'user_confirmed')
      if (io && typeof io.println === 'function') io.println('diagnose → issue → tests → plan')
      appendEntry(db, rt.sessionId, 'user', makeUserPayload(buildExploreUserMessage(userText)))
    } else {
      appendEntry(db, rt.sessionId, 'user', makeUserPayload(userText))
    }
  } else {
    appendEntry(db, rt.sessionId, 'user', makeUserPayload(userText))
  }

  // model and rt._model already resolved above for concurrent probe (#24)
  const currentPhase = getPhase(db, rt.conversationId)

  if (['lm_studio_local', 'openai_compatible', 'zhipu', 'ollama', 'nehanda'].includes(provider)) {
    await runSdlcOpenAICompat(db, rt, io, hookRt, provider, model, currentPhase)
  } else {
    await runSdlcAnthropic(db, rt, io, hookRt, model, currentPhase)
  }

  await runHooks(db, hookRt, 'Stop', { stop_hook_active: false })

  // Genuine end-of-turn completion: when the orchestration reached `done` (after
  // a real confirmed fix), persist the reset acknowledgement, then use the
  // ordinary protected `done -> idle` transition. Exploration cache is transient
  // and cleared here; durable memories/JEV evidence are intentionally retained.
  try {
    if (getPhase(db, rt.conversationId) === 'done') {
      withTransaction(db, () => {
        db.prepare(`INSERT INTO events (conversation_id, session_id, event_type, detail) VALUES (?, ?, 'reset_ack', ?)`).run(
          rt.conversationId,
          rt.sessionId || null,
          JSON.stringify({ reset: true, acknowledged: true, reason: 'turn completed' })
        )
      })
      try { resetExploreReadCacheTools(rt.conversationId) } catch {}
      setPhase(db, rt.conversationId, 'idle', 'reset_ack')
    }
  } catch (err) {
    try { io && typeof io.println === 'function' && io.println(`[completion] reset failed: ${err && err.message ? err.message : err}`) } catch {}
  }
}

async function runSdlcAnthropic(db, rt, io, hookRt, model, startPhase) {
  const cred = resolveAnthropicCredentials({ bareMode: rt.bareMode, apiKeyHelper: rt.settings?.apiKeyHelper ?? null })
  if (cred.mode === 'none') { io.println('No credentials found.'); return }
  const client = makeAnthropicClient(cred)
  const { anthropic: tools } = await toolsForProvider(activeProvider(rt.settings), rt.settings)

  let phase = startPhase

  if (phase === 'idle') {
    const text = await runAnthropicPhase(db, rt, io, hookRt, client, tools, phase)
    if (text) io.write(text)
    return
  }

  while (['explore', 'diagnose', 'create_issue', 'create_tests', 'planning', 'implement', 'verify', 'safety', 'confirm_fix'].includes(phase)) {
    const text = await runAnthropicPhase(db, rt, io, hookRt, client, tools, phase)
    if (text === null) return

    let advanced = false

    if (phase === 'explore' && (rt._exploreRequestBudgetExhausted||rt._exploreHandoffComplete)) {
      advanced = true
    } else if (phase === 'explore') {
      advanced = await exploreGateAnthropic(db, rt, io, client, rt._userText)
    } else if (phase === 'planning') {
      const __candidatePlan = text && String(text).trim() ? String(text).trim() : ''
      const __hasTriadPlan = !!__candidatePlan && validatePlanSchema(__candidatePlan) && planIssueReferenceMatches(db, rt, __candidatePlan)
      const __planText = __hasTriadPlan ? text : null
      advanced = __planText
        ? await planGate(db, rt, io, __planText)
        : await retryEmptyPlanningTurn(db, rt, io, () => runAnthropicPhase(db, rt, io, hookRt, client, tools, 'planning'))
    } else if (phase === 'implement') {
      const toolResults = extractRecentToolResults(db, rt.sessionId)
      const summary = await runAnthropicSummaryTurn(db, rt, io, hookRt, client, buildImplementSummaryPrompt(toolResults))
      advanced = await implementGate(db, rt, io, summary)
    } else if (phase === 'diagnose') {
      advanced = await diagnoseGate(db, rt, io, text)
    } else if (phase === 'create_issue') {
      advanced = await createIssueGate(db, rt, io, text)
    } else if (phase === 'create_tests') {
      advanced = await createTestsGate(db, rt, io, text)
    } else if (phase === 'verify') {
      advanced = await verifyGate(db, rt, io, text)
    } else if (phase === 'safety') {
      advanced = await safetyGate(db, rt, io, text)
    } else if (phase === 'confirm_fix') {
      advanced = await confirmFixGate(db, rt, io, text)
    }

    if (!advanced) break

    phase = getPhase(db, rt.conversationId)
    if (phase === 'idle') break

    if (phase === 'implement') {
      const plan = db.prepare(`SELECT content FROM plans WHERE conversation_id = ? AND status = 'approved' ORDER BY id DESC LIMIT 1`).get(rt.conversationId)
      appendEntry(db, rt.sessionId, 'user', makeUserPayload(buildImplementUserMessage(plan?.content || '')))
    } else if (phase === 'test') {
      const hasImplemented = db.prepare(`SELECT 1 FROM events WHERE conversation_id = ? AND event_type = 'phase' AND detail = 'implement' LIMIT 1`).get(rt.conversationId)
      if (!hasImplemented) {
        const plan = db.prepare(`SELECT content FROM plans WHERE conversation_id = ? AND status = 'approved' ORDER BY id DESC LIMIT 1`).get(rt.conversationId)
        appendEntry(db, rt.sessionId, 'user', makeUserPayload(buildCreateTestUserMessage(plan?.content || '')))
      } else {
        appendEntry(db, rt.sessionId, 'user', makeUserPayload(buildTestUserMessage()))
      }
    }
  }
}

async function runSdlcOpenAICompat(db, rt, io, hookRt, provider, model, startPhase) {
  if (io.spinner) io.spinner.start('Analyzing')
  let baseUrl, apiKey
  const savedBaseUrl = rt.settings?.model_config?.base_url?.replace(/\/+$/, '')
  if (provider === 'ollama') {
    baseUrl = savedBaseUrl || process.env.OLLAMA_BASE_URL || 'http://localhost:11434/v1'
    apiKey = 'ollama'
  } else if (provider === 'nehanda') {
    baseUrl = savedBaseUrl || process.env.NEHANDA_BASE_URL || 'https://nehanda-ml.asoba.co/v1'
    apiKey = rt.settings?.model_config?.api_key || process.env.NEHANDA_API_KEY || ''
  } else if (provider === 'zhipu') {
    baseUrl = savedBaseUrl || 'https://open.bigmodel.cn/api/paas/v4'
    apiKey = resolveZhipuApiKey()
  } else {
    // openai_compatible, lm_studio_local, and any future OpenAI-compat providers
    baseUrl = savedBaseUrl || process.env.OPENAI_BASE_URL
    apiKey = rt.settings?.model_config?.api_key || resolveOpenAiCompatKey()
  }

  const forceManual = rt.settings?.model_config?.force_manual_tools === true

  // #24: nativeSupport was already resolved concurrently with classifyUserIntent
  // in runUserTurn to avoid up to 3s sequential wait.
  let nativeSupport = rt._ollamaNativeSupport ?? true

  const { openai: allTools } = await toolsForProvider(provider, rt.settings, nativeSupport)
  const injectedTools = (forceManual || !nativeSupport) ? allTools : null

  let phase = startPhase

  if (phase === 'idle') {
    const text = await runOpenAICompatPhase(db, rt, io, hookRt, provider, baseUrl, apiKey, allTools, injectedTools, phase)
    if (text) io.write(text)
    return
  }

  while (['explore', 'diagnose', 'create_issue', 'create_tests', 'planning', 'implement', 'verify', 'safety', 'confirm_fix'].includes(phase)) {
    const text = await runOpenAICompatPhase(db, rt, io, hookRt, provider, baseUrl, apiKey, allTools, injectedTools, phase)
    if (text === null) return

    let advanced = false

    if (phase === 'explore' && (rt._exploreRequestBudgetExhausted||rt._exploreHandoffComplete)) {
      advanced = true
    } else if (phase === 'explore') {
      advanced = await exploreGateOpenAICompat(db, rt, io, provider, baseUrl, apiKey, rt._userText)
    } else if (phase === 'planning') {
      const __candidatePlan = text && String(text).trim() ? String(text).trim() : ''
      const __hasTriadPlan = !!__candidatePlan && validatePlanSchema(__candidatePlan) && planIssueReferenceMatches(db, rt, __candidatePlan)
      const __planText = __hasTriadPlan ? text : null
      advanced = __planText
        ? await planGate(db, rt, io, __planText)
        : await retryEmptyPlanningTurn(db, rt, io, () => runOpenAICompatPhase(db, rt, io, hookRt, provider, baseUrl, apiKey, allTools, injectedTools, 'planning'))
    } else if (phase === 'implement') {
      const toolResults = extractRecentToolResults(db, rt.sessionId)
      const summary = await runOpenAICompatSummaryTurn(db, rt, io, hookRt, provider, baseUrl, apiKey, buildImplementSummaryPrompt(toolResults))
      advanced = await implementGate(db, rt, io, summary)
    } else if (phase === 'diagnose') {
      advanced = await diagnoseGate(db, rt, io, text)
    } else if (phase === 'create_issue') {
      advanced = await createIssueGate(db, rt, io, text)
    } else if (phase === 'create_tests') {
      advanced = await createTestsGate(db, rt, io, text)
    } else if (phase === 'verify') {
      advanced = await verifyGate(db, rt, io, text)
    } else if (phase === 'safety') {
      advanced = await safetyGate(db, rt, io, text)
    } else if (phase === 'confirm_fix') {
      advanced = await confirmFixGate(db, rt, io, text)
    }

    if (!advanced) break

    phase = getPhase(db, rt.conversationId)
    if (phase === 'idle') break

    if (phase === 'implement') {
      const plan = db.prepare(`SELECT content FROM plans WHERE conversation_id = ? AND status = 'approved' ORDER BY id DESC LIMIT 1`).get(rt.conversationId)
      appendEntry(db, rt.sessionId, 'user', makeUserPayload(buildImplementUserMessage(plan?.content || '')))
    } else if (phase === 'test') {
      const hasImplemented = db.prepare(`SELECT 1 FROM events WHERE conversation_id = ? AND event_type = 'phase' AND detail = 'implement' LIMIT 1`).get(rt.conversationId)
      if (!hasImplemented) {
        const plan = db.prepare(`SELECT content FROM plans WHERE conversation_id = ? AND status = 'approved' ORDER BY id DESC LIMIT 1`).get(rt.conversationId)
        appendEntry(db, rt.sessionId, 'user', makeUserPayload(buildCreateTestUserMessage(plan?.content || '')))
      } else {
        appendEntry(db, rt.sessionId, 'user', makeUserPayload(buildTestUserMessage()))
      }
    }
  }
}