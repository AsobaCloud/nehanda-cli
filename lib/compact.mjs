import { appendEntry } from './transcript.mjs'
import { withTransaction, getLayaJevState, upsertLayaJevState, getPinnedContextBlocks } from './store.mjs'
import { runHooks } from './hookplane.mjs'
import { extractCanonicalState } from './laya.mjs'
import {
  transcriptTailText,
  extractRecentFilePaths,
  ingestObservation,
} from './jev_write.mjs'
import { retrieveEvidence } from './jev_read.mjs'

/**
 * Compact a conversation by summarizing old messages into a collapse_commit entry.
 * Old entries are NOT deleted — they remain for full recall.
 * The transcript builders detect collapse_commit and skip prior entries.
 *
 * When Jev-Mem is available, prefers a canonical System-1 header over free-text
 * System-2 prose summaries (see compactWithJev / compactConversationWithJev).
 */
export async function compactConversation(db, rt, summarize, log) {
  const hookRt = {
    sessionId: rt.sessionId, conversationId: rt.conversationId,
    runtimeDbPath: rt.runtimeDbPath, cwd: rt.cwd,
    permissionMode: rt.settings?.permissions?.defaultMode ?? 'default',
    settings: rt.settings,
  }

  const lastCollapse = db.prepare(
    `SELECT sequence FROM transcript_entries WHERE session_id = ? AND entry_type = 'collapse_commit' ORDER BY sequence DESC LIMIT 1`
  ).get(rt.sessionId)
  const startSeq = lastCollapse ? lastCollapse.sequence + 1 : 0

  const entries = db.prepare(
    `SELECT entry_type, payload_json FROM transcript_entries WHERE session_id = ? AND sequence >= ? AND entry_type != 'collapse_commit' ORDER BY sequence ASC`
  ).all(rt.sessionId, startSeq)

  if (entries.length < 4) {
    log('Not enough messages to compact.')
    return null
  }

  await runHooks(db, hookRt, 'PreCompact', { trigger: 'manual' })

  // Fetch pinned blocks — these are never passed to the distillation summariser.
  const pinnedBlocks = getPinnedContextBlocks(db, rt.sessionId)

  log('Compacting conversation (Jev-Mem)...')
  let summary
  try {
    summary = await buildJevCollapseSummary(db, rt, pinnedBlocks)
  } catch {
    const text = entries.map(e => {
      try {
        const p = JSON.parse(e.payload_json)
        if (e.entry_type === 'user') return `User: ${extractText(p)}`
        if (e.entry_type === 'assistant') return `Assistant: ${extractText(p)}`
        if (e.entry_type === 'tool_result') return `Tool result: ${p.content?.slice?.(0, 200) || ''}`
        return ''
      } catch { return '' }
    }).filter(Boolean).join('\n')
    log('Jev compact failed; falling back to summarizer.')
    summary = await summarize(text)
    // Reattach pinned blocks verbatim even on fallback path.
    if (pinnedBlocks.length) {
      summary += '\n\n' + formatPinnedBlocksSection(pinnedBlocks)
    }
  }

  withTransaction(db, () => {
    appendEntry(db, rt.sessionId, 'collapse_commit', {
      _t: 'collapse_commit',
      summary,
      compacted_count: entries.length,
      compacted_from_sequence: startSeq,
      jev: true,
      pinned_keys: pinnedBlocks.map(b => b.key),
    })

    db.prepare(
      `INSERT OR REPLACE INTO summaries(conversation_id, content, word_count) VALUES (?, ?, ?)`
    ).run(rt.conversationId, summary, summary.split(/\s+/).length)
  })

  await runHooks(db, hookRt, 'PostCompact', { trigger: 'manual', compact_summary: summary })

  log(`Compacted ${entries.length} entries.`)
  return summary
}

async function buildJevCollapseSummary(db, rt, pinnedBlocks = []) {
  const paths = extractRecentFilePaths(db, rt.sessionId)
  const tail = transcriptTailText(db, rt.sessionId)
  const state = await extractCanonicalState(tail, paths, rt.layaOpts || {})
  upsertLayaJevState(db, rt.sessionId, state)
  const retrieved = await retrieveEvidence(db, tail || 'continue the current task', {
    sessionId: rt.sessionId,
    conversationId: rt.conversationId,
    layaOpts: rt.layaOpts || {},
  })
  let summary = formatCanonicalHeader(state) + '\n' + formatEvidenceBlock(retrieved.evidence)
  if (pinnedBlocks.length) {
    summary += '\n\n' + formatPinnedBlocksSection(pinnedBlocks)
  }
  return summary
}

export function formatCanonicalHeader(layaState) {
  if (!layaState) {
    return `[CANONICAL SYSTEM 1 STATE (Laya)]\n- Task Status: unknown\n- Target File: (none)\n- Escalation Flag: FALSE\n--------------------------------------------------`
  }
  const esc = layaState.escalation_risk ? 'TRUE' : 'FALSE'
  const conf = Number(layaState.confidence_score || 0)
  const confStr = Number.isFinite(conf) ? ` (p=${conf.toFixed(2)})` : ''
  return [
    '[CANONICAL SYSTEM 1 STATE (Laya 421M)]',
    `- Task Status: ${layaState.task_status || 'investigating'}${confStr}`,
    `- Target File: ${layaState.file_target || '(none)'}`,
    `- Escalation Flag: ${esc}`,
    '--------------------------------------------------',
  ].join('\n')
}

export function formatEvidenceBlock(evidence = []) {
  if (!evidence.length) return '[RETRIEVED EVIDENCE]\n(none)\n--------------------------------------------------'
  const lines = evidence.slice(0, 6).map((e, i) => {
    const body = String(e.content || '').replace(/\s+/g, ' ').slice(0, 160)
    return `${i + 1}. (${e.via || 'mem'}) ${e.title || e.id}: ${body}`
  })
  return ['[RETRIEVED EVIDENCE]', ...lines, '--------------------------------------------------'].join('\n')
}

/**
 * Serialise pinned context blocks into a labelled section that is appended
 * verbatim to the collapse_commit summary — never distilled.
 */
export function formatPinnedBlocksSection(blocks = []) {
  if (!blocks.length) return ''
  const lines = ['[PINNED DOMAIN INVARIANTS]']
  for (const b of blocks) {
    lines.push(`-- ${b.key} --`)
    lines.push(String(b.content || ''))
  }
  lines.push('--------------------------------------------------')
  return lines.join('\n')
}

/**
 * Rough token estimate (~4 chars/token) for behavioral budgets.
 */
export function estimateTokens(text) {
  return Math.ceil(String(text || '').length / 4)
}

export function estimateMessagesTokens(messages) {
  let n = 0
  for (const m of messages || []) {
    if (typeof m.content === 'string') n += estimateTokens(m.content)
    else if (Array.isArray(m.content)) {
      for (const b of m.content) {
        if (typeof b === 'string') n += estimateTokens(b)
        else if (b?.text) n += estimateTokens(b.text)
        else if (b?.content) n += estimateTokens(typeof b.content === 'string' ? b.content : JSON.stringify(b.content))
        else n += estimateTokens(JSON.stringify(b))
      }
    } else if (m.tool_calls) {
      n += estimateTokens(JSON.stringify(m.tool_calls))
    }
  }
  return n
}

/**
 * Find index ranges for "last N user turns" in an OpenAI-style message list.
 * A turn starts at a user message that is not solely tool_result packing.
 */
export function selectTrailingMessages(messages, { turns = 2, includeLastFailedTool = true } = {}) {
  if (!Array.isArray(messages) || !messages.length) return []
  const userIdxs = []
  for (let i = 0; i < messages.length; i++) {
    if (messages[i].role === 'user') userIdxs.push(i)
  }
  const startUser = userIdxs.length <= turns
    ? (userIdxs[0] ?? 0)
    : userIdxs[userIdxs.length - turns]

  let slice = messages.slice(startUser)

  if (includeLastFailedTool) {
    let lastFail = -1
    for (let i = 0; i < messages.length; i++) {
      const m = messages[i]
      if (m.role === 'tool' && isFailedToolContent(m.content)) lastFail = i
      if (m.role === 'user' && Array.isArray(m.content)) {
        for (const b of m.content) {
          if (b?.type === 'tool_result' && b.is_error) lastFail = i
        }
      }
    }
    if (lastFail >= 0 && lastFail < startUser) {
      // Include the failed tool message and its preceding assistant tool_call if present
      let from = lastFail
      if (from > 0 && messages[from - 1]?.role === 'assistant') from = from - 1
      const failSlice = messages.slice(from, lastFail + 1)
      // Dedupe if already inside trailing slice
      const ids = new Set(slice.map((_, i) => startUser + i))
      const extra = []
      for (let i = from; i <= lastFail; i++) {
        if (!ids.has(i)) extra.push(messages[i])
      }
      slice = [...extra, ...slice]
      void failSlice
    }
  }

  return slice
}

function isFailedToolContent(content) {
  const s = typeof content === 'string' ? content : JSON.stringify(content ?? '')
  return /\b(FAILED|is_error\s*[:=]\s*true|exit code [1-9]\d*|permission denied|ENOENT|Command failed)\b/i.test(s)
}

/**
 * Build System-2 message list: pinned blocks + canonical header + evidence + last 2 turns + last failed tool.
 * Tool payloads in the trailing window are capped so prefill stays bounded.
 * pinnedBlocks rows come from getPinnedContextBlocks — they are prepended verbatim before the
 * canonical header so they are never truncated by the trailing-message window.
 */
export function compactWithJev(messages, layaState, evidence = [], opts = {}) {
  const header = formatCanonicalHeader(layaState)
  const ev = formatEvidenceBlock(evidence)
  const pinnedBlocks = opts.pinnedBlocks || []

  // Pinned invariants are injected first — outside the truncation budget.
  const pinnedSection = formatPinnedBlocksSection(pinnedBlocks)
  const preamble = pinnedSection
    ? `${pinnedSection}\n\n${header}\n${ev}`
    : `${header}\n${ev}`

  const trailing = selectTrailingMessages(messages, {
    turns: opts.turns ?? 2,
    includeLastFailedTool: opts.includeLastFailedTool !== false,
  })

  const body = truncateTrailingPayloads(trailing.length ? trailing : (messages || []).slice(-4), opts)

  return [
    { role: 'user', content: preamble },
    { role: 'assistant', content: 'Acknowledged. Continuing from canonical System-1 state and retrieved evidence.' },
    ...body,
  ]
}

const TOOL_CAP_CHARS = 280
const FAILED_TOOL_CAP_CHARS = 480
const TEXT_CAP_CHARS = 400

function truncateTrailingPayloads(messages, opts = {}) {
  const toolCap = opts.toolCapChars ?? TOOL_CAP_CHARS
  const failCap = opts.failedToolCapChars ?? FAILED_TOOL_CAP_CHARS
  const textCap = opts.textCapChars ?? TEXT_CAP_CHARS

  return (messages || []).map(m => {
    if (m.role === 'tool') {
      const raw = typeof m.content === 'string' ? m.content : JSON.stringify(m.content ?? '')
      const cap = isFailedToolContent(raw) ? failCap : toolCap
      if (raw.length <= cap) return m
      return { ...m, content: raw.slice(0, cap) + `\n... [truncated ${raw.length} chars]` }
    }
    if (m.role === 'user' || m.role === 'assistant') {
      if (typeof m.content === 'string' && m.content.length > textCap) {
        return { ...m, content: m.content.slice(0, textCap) + '\n... [truncated]' }
      }
      if (Array.isArray(m.content)) {
        return {
          ...m,
          content: m.content.map(b => {
            if (b?.type === 'tool_result') {
              const raw = typeof b.content === 'string' ? b.content : JSON.stringify(b.content ?? '')
              const cap = b.is_error ? failCap : toolCap
              if (raw.length <= cap) return b
              return { ...b, content: raw.slice(0, cap) + `\n... [truncated ${raw.length} chars]` }
            }
            if (b?.type === 'text' && typeof b.text === 'string' && b.text.length > textCap) {
              return { ...b, text: b.text.slice(0, textCap) + '\n... [truncated]' }
            }
            return b
          }),
        }
      }
    }
    return m
  })
}

/**
 * Hot-path: refresh System-1 state + retrieve evidence + compact messages.
 * On any failure, returns windowed truncation of the original messages.
 */
export async function buildJevSystem2Messages(db, rt, messages, opts = {}) {
  const turns = opts.turns ?? 2
  try {
    const paths = extractRecentFilePaths(db, rt.sessionId)
    const tail = transcriptTailText(db, rt.sessionId)
    const layaOpts = opts.layaOpts || rt.layaOpts || {}

    // Write-path: ingest latest tail as an observation (preserve; cheap when degraded)
    if (opts.ingest !== false && tail) {
      await ingestObservation(db, {
        sessionId: rt.sessionId,
        conversationId: rt.conversationId,
        text: tail,
        title: `turn:${Date.now()}`,
        provenance: 'hot_path',
        filePaths: paths,
        layaOpts,
        updateSessionState: true,
      })
    } else {
      const extracted = await extractCanonicalState(tail, paths, layaOpts)
      upsertLayaJevState(db, rt.sessionId, extracted)
    }

    const state = getLayaJevState(db, rt.sessionId)
    const retrieved = await retrieveEvidence(db, opts.query || tail || 'continue', {
      sessionId: rt.sessionId,
      conversationId: rt.conversationId,
      layaOpts,
    })

    // Pinned blocks are always reattached — they survive every compaction cycle.
    const pinnedBlocks = getPinnedContextBlocks(db, rt.sessionId)

    return compactWithJev(messages, state, retrieved.evidence, { turns, pinnedBlocks })
  } catch {
    // Resilience: rolling window fallback — never crash the CLI
    return selectTrailingMessages(messages, { turns, includeLastFailedTool: true })
  }
}

function extractText(payload) {
  const blocks = payload.content
  if (!Array.isArray(blocks)) return ''
  return blocks.filter(b => b?.type === 'text' && typeof b.text === 'string').map(b => b.text).join('\n')
}
