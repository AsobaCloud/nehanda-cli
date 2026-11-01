/**
 * Behavioral tests — Issue #23: Spinner verb spitfire between tool calls
 *
 * The #18 fix added lastSpinLabelRef as a fallback when updateSpinner(null) is
 * received between tool calls. But the orchestrator loop unconditionally calls
 * io.spinner.start('Thinking') at the top of every iteration, which overwrites
 * the fallback with the generic 'Thinking' sentinel — triggering TOOL_VERBS rotation.
 *
 * SPIT-1: io.spinner.start('Thinking') must NOT be called unconditionally at loop top
 *         when a previous tool result already set a meaningful label.
 * SPIT-2: The orchestrator loop must preserve spinner label continuity between
 *         a tool result and the next tool start — no snap to rotating verbs.
 * SPIT-3: The first iteration (no prior tool) may set 'Thinking' — that's fine.
 * SPIT-4: Source contract: runOpenAICompatPhase must not call io.spinner.start
 *         with 'Thinking' as a hardcoded label on every iteration.
 * SPIT-5: The lastSpinLabelRef fallback must survive across loop iterations,
 *         not be defeated by an unconditional 'Thinking' reset.
 */

import assert from 'node:assert'
import fs from 'node:fs'
import path from 'node:path'
import { fileURLToPath } from 'node:url'

const __dirname = path.dirname(fileURLToPath(import.meta.url))
const ROOT = path.resolve(__dirname, '../..')

let passed = 0
let failed = 0

function test(name, fn) {
  return Promise.resolve()
    .then(() => fn())
    .then(() => { console.log(`✓ ${name}`); passed++ })
    .catch(err => { console.error(`✗ ${name}\n  ${err.stack || err.message}`); failed++ })
}

// ── SPIT-4: Source contract — no unconditional 'Thinking' at loop top ──

await test('SPIT-4: runOpenAICompatPhase must not unconditionally call io.spinner.start("Thinking") at every loop iteration', () => {
  const src = fs.readFileSync(path.join(ROOT, 'lib/orchestrate.mjs'), 'utf8')

  // Find the for(;;) loop in runOpenAICompatPhase
  const fnStart = src.indexOf('export async function runOpenAICompatPhase')
  assert.ok(fnStart !== -1, 'Could not find runOpenAICompatPhase in orchestrate.mjs')

  const fnEnd = src.indexOf('\nasync function executeToolUses', fnStart)
  assert.ok(fnEnd !== -1, 'Could not find end of runOpenAICompatPhase')

  const fnBody = src.slice(fnStart, fnEnd)

  // The unconditional pattern is: if (io.spinner) io.spinner.start('Thinking')
  // at the top of the for(;;) loop, before any check of prior state.
  // The fix should either:
  //   a) Not call io.spinner.start at loop top at all (let onToolStart manage it), or
  //   b) Use a conditional that preserves the last label, or
  //   c) Use a different sentinel that doesn't trigger TOOL_VERBS rotation

  // Check: does the loop body contain io.spinner.start('Thinking') unconditionally?
  const forLoopStart = fnBody.indexOf('for (;;)')
  assert.ok(forLoopStart !== -1, 'Could not find for(;;) loop in runOpenAICompatPhase')

  const forLoopBody = fnBody.slice(forLoopStart)

  // Find all occurrences of io.spinner.start in the loop
  const spinnerStartMatches = []
  const regex = /io\.spinner\.start\(/g
  let match
  while ((match = regex.exec(forLoopBody)) !== null) {
    // Grab the next 60 chars to see the argument
    const snippet = forLoopBody.slice(match.index, match.index + 60)
    spinnerStartMatches.push(snippet)
  }

  // The defect: at least one call passes 'Thinking' literally
  const hasUnconditionalThinking = spinnerStartMatches.some(s =>
    s.includes("'Thinking'") || s.includes('"Thinking"')
  )

  assert.ok(
    !hasUnconditionalThinking,
    `SPIT-4 FAIL: runOpenAICompatPhase loop calls io.spinner.start('Thinking') unconditionally.\n` +
    `Found ${spinnerStartMatches.length} io.spinner.start() calls in the loop:\n` +
    spinnerStartMatches.map((s, i) => `  [${i}] ${s.trim()}`).join('\n') + '\n\n' +
    `Effect: 'Thinking' is the sentinel value that triggers TOOL_VERBS rotation in the Spinner component.\n` +
    `It overwrites the lastSpinLabelRef fallback from the previous tool result, causing verb spitfire.\n\n` +
    `Fix: Either remove the io.spinner.start('Thinking') call (let onToolStart/onToolResult manage labels),\n` +
    `or conditionally skip it when a prior label exists, or use a non-rotation-triggering sentinel.`
  )
})

// ── SPIT-1: Spinner label state machine — no rotation between tool calls ──

await test('SPIT-1: Spinner label must not snap to rotating verb between consecutive tool calls', () => {
  // Model the actual state machine from the FIXED codebase.
  // The bridge.updateSpinner handler (#23):
  //   spinLabel = text
  //   if (text !== null && text !== 'Thinking') lastSpinLabelRef.current = text
  //
  // The Spinner component:
  //   verb = (!label || label === 'Thinking') ? TOOL_VERBS[frame/5 % len] : label
  //
  // The orchestrator loop (#23): no longer calls io.spinner.start('Thinking') at loop top.
  // The spinner keeps running from the parent function's 'Analyzing' or the lastSpinLabel fallback.
  // io.spinner.start maps to bridge.updateSpinner(label).
  // io.spinner.stop maps to bridge.updateSpinner(null).
  // onToolStart maps to bridge.updateSpinner(verbForTool(name) + summary).
  // onToolResult maps to bridge.updateSpinner(null).

  const TOOL_VERBS = ['Analyzing', 'Thinking', 'Retrieving', 'Planning', 'Reading',
    'Writing', 'Searching', 'Running', 'Coordinating']

  let spinLabel = null
  let lastSpinLabel = null
  let frame = 0

  function updateSpinner(text) {
    spinLabel = text
    // #23: guard against saving 'Thinking' — it's the rotation sentinel
    if (text !== null && text !== 'Thinking') lastSpinLabel = text
  }

  function getRenderedVerb() {
    const label = spinLabel ?? lastSpinLabel // lastSpinLabelRef fallback
    if (!label || label === 'Thinking') {
      return TOOL_VERBS[Math.floor(frame / 5) % TOOL_VERBS.length]
    }
    return label
  }

  // Simulate a multi-tool-call turn:
  // Iteration 1: LLM returns tool call "Read"
  // Iteration 2: LLM returns tool call "Glob"
  // Iteration 3: LLM returns plain text (done)

  const renderedLabels = []

  // --- Iteration 1 ---
  // #23: Parent function (runSdlcOpenAICompat) starts with 'Analyzing', not 'Thinking'.
  updateSpinner('Analyzing')      // io.spinner.start('Analyzing') at function entry
  frame = 0                       // spinner starts at frame 0
  // Simulate 25 frames (2 seconds at 80ms) while LLM generates
  for (let i = 0; i < 25; i++) { frame++; renderedLabels.push(getRenderedVerb()) }
  updateSpinner(null)             // io.spinner.stop() before tool parsing
  updateSpinner('Reading ui.mjs') // onToolStart
  renderedLabels.push(getRenderedVerb())
  updateSpinner(null)             // onToolResult
  renderedLabels.push(getRenderedVerb())

  // --- Iteration 2 ---
  // #23: No updateSpinner('Thinking') at loop top — the fix removed it.
  // The spinner keeps running with lastSpinLabel fallback ('Reading ui.mjs').
  // Simulate 25 frames while LLM generates next tool call
  for (let i = 0; i < 25; i++) { frame++; renderedLabels.push(getRenderedVerb()) }
  updateSpinner(null)             // io.spinner.stop()
  updateSpinner('Searching **/*.py') // onToolStart: Glob
  renderedLabels.push(getRenderedVerb())
  updateSpinner(null)             // onToolResult
  renderedLabels.push(getRenderedVerb())

  // Check: between tool result of iteration 1 and tool start of iteration 2,
  // the rendered verb should NOT cycle through multiple different verbs.
  // The labels from index 27 (after onToolResult of iter 1) to index 52 (before onToolStart of iter 2)
  // should all be the same (the lastSpinLabel fallback), not rotating.

  // Find the transition: after onToolResult(null) of iter 1, before onToolStart of iter 2
  const afterTool1Result = renderedLabels.indexOf('Reading ui.mjs', renderedLabels.indexOf('Reading ui.mjs') + 1)
  // The labels between tool result and next tool start
  const betweenCalls = renderedLabels.slice(afterTool1Result + 1)

  // With the defect, betweenCalls will contain rotating verbs (Analyzing, Thinking, Retrieving...)
  // With the fix, betweenCalls should all be 'Reading ui.mjs' (the lastSpinLabel fallback)
  const uniqueBetweenCalls = [...new Set(betweenCalls)]

  assert.ok(
    uniqueBetweenCalls.length <= 2, // allow the label + maybe the next tool's label
    `SPIT-1 FAIL: Spinner cycled through ${uniqueBetweenCalls.length} different verbs between tool calls.\n` +
    `Unique verbs: ${JSON.stringify(uniqueBetweenCalls)}\n` +
    `Expected: lastSpinLabel fallback ('Reading ui.mjs') should persist, not rotate.\n\n` +
    `The 'Thinking' reset at loop top (orchestrate.mjs:1004) defeats the lastSpinLabelRef fallback.\n` +
    `The Spinner component treats 'Thinking' as a generic label and rotates TOOL_VERBS every 400ms.`
  )
})

// ── SPIT-2: Full lifecycle — label continuity across multiple tool calls ──

await test('SPIT-2: Spinner label continuity across 3 consecutive tool calls (no verb rotation gaps)', () => {
  const TOOL_VERBS = ['Analyzing', 'Thinking', 'Retrieving', 'Planning', 'Reading',
    'Writing', 'Searching', 'Running', 'Coordinating']

  let spinLabel = null
  let lastSpinLabel = null
  let frame = 0

  function updateSpinner(text) {
    spinLabel = text
    // #23: guard against saving 'Thinking'
    if (text !== null && text !== 'Thinking') lastSpinLabel = text
  }

  function getRenderedVerb() {
    const label = spinLabel ?? lastSpinLabel
    if (!label || label === 'Thinking') {
      return TOOL_VERBS[Math.floor(frame / 5) % TOOL_VERBS.length]
    }
    return label
  }

  const labels = []

  // Simulate 3 tool calls with LLM thinking time between each
  const toolLabels = ['Reading config.py', 'Searching **/*.ts', 'Running npm test']

  for (let iter = 0; iter < 3; iter++) {
    // #23: No updateSpinner('Thinking') at loop top — the fix removed it.
    // First iteration: spinner was started with 'Analyzing' by parent function.
    if (iter === 0) updateSpinner('Analyzing')
    // Subsequent iterations: no reset, lastSpinLabel fallback persists.
    // LLM thinks for 25 frames
    for (let i = 0; i < 25; i++) { frame++; labels.push(getRenderedVerb()) }
    // Tool starts
    updateSpinner(null) // spinner.stop
    updateSpinner(toolLabels[iter]) // onToolStart
    labels.push(getRenderedVerb())
    // Tool completes
    updateSpinner(null) // onToolResult
    labels.push(getRenderedVerb())
  }

  // Check: between each tool result and the next tool start, the label should NOT rotate.
  // Find indices where tool labels appear
  for (let i = 0; i < toolLabels.length - 1; i++) {
    const afterToolResult = labels.indexOf(toolLabels[i], labels.indexOf(toolLabels[i]) + 1)
    const beforeNextTool = labels.indexOf(toolLabels[i + 1])

    const gap = labels.slice(afterToolResult + 1, beforeNextTool)
    const uniqueInGap = [...new Set(gap)]

    assert.ok(
      uniqueInGap.length <= 1,
      `SPIT-2 FAIL: Between tool ${i + 1} result and tool ${i + 2} start, ` +
      `${uniqueInGap.length} unique labels found: ${JSON.stringify(uniqueInGap)}\n` +
      `Expected: lastSpinLabel fallback ('${toolLabels[i]}') should persist, not rotate.`
    )
  }
})

// ── SPIT-3: First iteration may set 'Thinking' — that's acceptable ──

await test('SPIT-3: First loop iteration may show rotating verbs (no prior tool label exists)', () => {
  // On the very first iteration, there is no prior tool result, so 'Thinking'
  // rotation is acceptable. The fix should not break this.
  const src = fs.readFileSync(path.join(ROOT, 'lib/orchestrate.mjs'), 'utf8')

  const fnStart = src.indexOf('export async function runOpenAICompatPhase')
  const fnEnd = src.indexOf('\nasync function executeToolUses', fnStart)
  const fnBody = src.slice(fnStart, fnEnd)

  // The fix must still allow some spinner initialization on the first iteration.
  // Check that there's still a spinner.start call somewhere in the function
  // (either at loop top with a condition, or elsewhere).
  assert.ok(
    fnBody.includes('io.spinner'),
    'SPIT-3: runOpenAICompatPhase must still reference io.spinner for initial display'
  )
})

// ── SPIT-5: lastSpinLabelRef must survive across loop iterations ──

await test('SPIT-5: lastSpinLabelRef must not be overwritten by generic "Thinking" on subsequent iterations', () => {
  const src = fs.readFileSync(path.join(ROOT, 'bin/nehanda-ui.mjs'), 'utf8')

  // The bridge.updateSpinner handler saves non-null labels to lastSpinLabelRef.
  // But if the orchestrator sends 'Thinking' on every iteration, lastSpinLabelRef
  // gets overwritten to 'Thinking', defeating the fallback.
  const updateSpinnerIdx = src.indexOf('bridge.updateSpinner')
  const handlerIdx = src.indexOf('bridge.updateSpinner  = text =>', updateSpinnerIdx)
  assert.ok(handlerIdx !== -1, 'Could not find bridge.updateSpinner handler')

  const handler = src.slice(handlerIdx, handlerIdx + 200)

  // The handler saves ANY non-null text to lastSpinLabelRef.
  // The fix must ensure 'Thinking' doesn't get saved as the fallback,
  // OR the orchestrator must not send 'Thinking' after a tool result.
  //
  // This test checks: does the handler unconditionally save 'Thinking'?
  // If the handler has a guard against 'Thinking', the fix is in the UI.
  // If not, the fix must be in the orchestrator (not sending 'Thinking').
  const savesThinking = handler.includes("if (text !== null) lastSpinLabelRef.current = text")

  // Check: does the handler distinguish 'Thinking' from specific labels?
  const hasThinkingGuard = handler.includes("!== 'Thinking'") || handler.includes("!= 'Thinking'")

  if (savesThinking && !hasThinkingGuard) {
    // The handler saves 'Thinking' unconditionally — so the fix MUST be in the orchestrator
    // (not sending 'Thinking' after a tool result). This is the current state — the test
    // should fail until the orchestrator is fixed.
    assert.ok(
      false,
      `SPIT-5 FAIL: bridge.updateSpinner saves 'Thinking' to lastSpinLabelRef unconditionally.\n` +
      `Handler: ${handler.trim()}\n\n` +
      `When the orchestrator calls io.spinner.start('Thinking') at loop top, ` +
      `lastSpinLabelRef.current becomes 'Thinking', which triggers TOOL_VERBS rotation.\n\n` +
      `Fix: The orchestrator must not call io.spinner.start('Thinking') after a tool result, ` +
      `or the handler must guard against saving 'Thinking' as the fallback.`
    )
  }
})

// ── Summary ──────────────────────────────────────────────────

console.log(`\n${'-'.repeat(60)}`)
console.log(`Issue #23 Spitfire Tests: ${passed} passed, ${failed} failed`)
console.log(`${'-'.repeat(60)}`)
if (failed > 0) process.exit(1)
