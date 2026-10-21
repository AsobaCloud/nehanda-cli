#!/usr/bin/env node
/**
 * Behavioral tests — /model wizard input hygiene + large-list navigation (#10)
 *
 * HP-1a: CREATED — consecutive wizard questions must not share input state.
 *        Every bridge.ask() gets a unique id (React key) so AskPrompt remounts
 *        and starts empty. Enter-alone on the host prompt then accepts the
 *        preset default host (regression guard).
 * HP-1b: CREATED — secret answers (API keys) are masked when persisted to
 *        transcript history; the raw key is never rendered. The wizard threads
 *        {secret:true} for key asks ONLY (host/custom-name asks stay unmasked).
 * HP-2:  CREATED — model lists above the picker threshold render in a picker:
 *        vendor groups (size desc) with the custom entry pinned last,
 *        case-insensitive type-to-filter search, centered windowed rows with
 *        hidden-above/below counts.
 */

import assert from 'node:assert/strict'
import {
  PICKER_THRESHOLD,
  createAskIdSequencer,
  groupModelsByVendor,
  filterModelItems,
  visibleWindow,
} from '../../lib/modelPicker.mjs'
import { maskSecretAnswer } from '../../lib/ui.mjs'
import { runConfigureWizard } from '../../lib/configureProvider.mjs'

let passed = 0
let failed = 0

function test(name, fn) {
  return Promise.resolve()
    .then(() => fn())
    .then(() => { console.log(`✓ ${name}`); passed++ })
    .catch(err => { console.error(`✗ ${name}\n  ${err.stack || err.message}`); failed++ })
}

const makeItem = name => ({ id: name, name, label: name, desc: 'Discovered', endpoint: 'https://openrouter.ai/api/v1' })
const CUSTOM = { id: '__custom__', name: '__custom__', label: '(enter custom name…)', desc: 'Type a model id not listed above', endpoint: 'https://openrouter.ai/api/v1' }

console.log('Running /model wizard input + picker behavioral tests (#10)…\n')

// ── HP-1a ─────────────────────────────────────────────────────────────────────

await test('HP-1a: ask-id sequencer yields distinct, strictly increasing ids', () => {
  const seq = createAskIdSequencer()
  const a = seq.next()
  const b = seq.next()
  const c = seq.next()
  assert.ok(Number.isInteger(a), 'ids are integers (React keyable)')
  assert.ok(a < b && b < c, `ids strictly increase (${a}, ${b}, ${c}) — distinct React keys remount AskPrompt`)
})

await test('HP-1a: independent sequencers do not share counters', () => {
  const s1 = createAskIdSequencer()
  const s2 = createAskIdSequencer()
  assert.strictEqual(s1.next(), 1)
  assert.strictEqual(s2.next(), 1)
})

// ── HP-1b ─────────────────────────────────────────────────────────────────────

await test('HP-1b: maskSecretAnswer never renders raw key material', () => {
  const raw = 'sk-or-v1-0123456789abcdefABCDEF'
  const masked = maskSecretAnswer(raw)
  assert.ok(!masked.includes(raw), 'raw key never appears in masked output')
  assert.ok(!masked.includes('sk-or'), 'no key prefix leaks')
  assert.ok(masked.includes('•••'), 'masked form is visibly marked')
})

await test('HP-1b: maskSecretAnswer handles empty/whitespace answers', () => {
  assert.strictEqual(maskSecretAnswer(''), '')
  assert.strictEqual(maskSecretAnswer('   '), '')
})

await test('HP-1b: wizard marks ONLY the API-key ask as secret; host/custom asks stay unmasked', async () => {
  const asks = []          // { q, opts }
  const keySaves = []
  const persisted = []

  const result = await runConfigureWizard({
    ask: async (q, opts) => {
      asks.push({ q, opts })
      if (/api key/i.test(q)) return 'sk-or-secret-XYZ'
      if (/host/i.test(q)) return ''        // Enter-alone → default host (HP-1a regression)
      if (/custom|model name/i.test(q)) return 'my/custom-model'
      return ''
    },
    select: async (title, items) => {
      if (/model/i.test(title)) return items.find(i => i.id === '__custom__')
      return items[0]
    },
    notify: () => {},
    discover: async () => ({
      success: true,
      models: [makeItem('openai/gpt-4o'), makeItem('anthropic/claude-sonnet-4')],
    }),
    saveKey: async ({ slot, key }) => { keySaves.push({ slot, key }) },
    resolveExistingKey: async () => '',
    persist: async cfg => { persisted.push(cfg) },
  }, { presetId: 'openrouter' })

  assert.ok(result, 'wizard completes')
  // Key ask: secret
  const keyAsk = asks.find(a => /api key/i.test(a.q))
  assert.ok(keyAsk, 'key ask happened')
  assert.strictEqual(keyAsk.opts?.secret, true, 'key ask carries {secret:true}')
  // Host ask: not secret
  const hostAsk = asks.find(a => /host/i.test(a.q))
  assert.ok(hostAsk, 'host ask happened')
  assert.ok(!hostAsk.opts?.secret, 'host ask is not marked secret')
  // Custom-name ask: not secret
  const customAsk = asks.find(a => /custom|model name/i.test(a.q))
  assert.ok(customAsk, 'custom name ask happened')
  assert.ok(!customAsk.opts?.secret, 'custom name ask is not marked secret')
  // Raw key still reaches the real consumer (saveKey), never the display path
  assert.strictEqual(keySaves[0]?.key, 'sk-or-secret-XYZ')
  assert.strictEqual(keySaves[0]?.slot, 'openai_compat')
  assert.strictEqual(result.model, 'my/custom-model')
  // HP-1a regression: empty host answer → preset default host, not the key
  assert.strictEqual(result.endpoint, 'https://openrouter.ai/api/v1')
  assert.notStrictEqual(result.endpoint, 'sk-or-secret-XYZ')
  assert.strictEqual(persisted.length, 1)
})

// ── HP-2 ─────────────────────────────────────────────────────────────────────

await test('HP-2: picker threshold is exported and pinned', () => {
  assert.strictEqual(typeof PICKER_THRESHOLD, 'number')
  assert.strictEqual(PICKER_THRESHOLD, 12)
})

await test('HP-2: grouping sorts by size desc then vendor, pins custom entry last', () => {
  const items = [
    makeItem('anthropic/claude-sonnet-4'),
    makeItem('openai/gpt-4o'),
    makeItem('openai/gpt-4o-mini'),
    makeItem('google/gemini-2.0-flash'),
    CUSTOM,
  ]
  const groups = groupModelsByVendor(items)
  const ids = groups.map(g => g.id)
  assert.strictEqual(ids[0], 'openai', 'largest vendor first')
  assert.ok(ids.indexOf('anthropic') < ids.indexOf('google'), 'ties alphabetical')
  assert.strictEqual(ids[ids.length - 1], '__custom__', 'custom entry pinned last')
  const openai = groups[0]
  assert.strictEqual(openai.models.length, 2)
  assert.strictEqual(openai.desc, '2 models')
  assert.ok(groups.some(g => g.desc === '1 model'), 'singular count text')
})

await test('HP-2: ollama-style vendor:tag ids group by tag prefix', () => {
  const groups = groupModelsByVendor([
    makeItem('llama3:8b'),
    makeItem('llama3:70b'),
    makeItem('qwen2.5:14b'),
  ])
  const llama3 = groups.find(g => g.id === 'llama3')
  assert.ok(llama3, 'llama3 group exists')
  assert.deepStrictEqual(llama3.models.map(m => m.name), ['llama3:8b', 'llama3:70b'])
})

await test('HP-2: separator-less ids form their own single-item group', () => {
  const groups = groupModelsByVendor([makeItem('gemma2')])
  assert.strictEqual(groups[0].id, 'gemma2')
  assert.strictEqual(groups[groups.length - 1].id, '__custom__')
})

await test('HP-2: filter is case-insensitive substring; custom entry always pinned last', () => {
  const items = [
    makeItem('openai/gpt-4o'),
    makeItem('anthropic/claude-sonnet-4'),
    makeItem('google/gemini-2.0-flash'),
    CUSTOM,
  ]
  const gpt = filterModelItems(items, 'GPT')
  assert.deepStrictEqual(gpt.map(i => i.name), ['openai/gpt-4o', '__custom__'])
  const none = filterModelItems(items, 'zzz-no-such-model')
  assert.deepStrictEqual(none.map(i => i.id), ['__custom__'], 'no matches → custom still reachable')
  const all = filterModelItems(items, '')
  assert.strictEqual(all.length, 4, 'empty filter returns everything + custom')
  assert.strictEqual(all[all.length - 1].id, '__custom__')
})

await test('HP-2: centered window exposes hidden-above/below counts', () => {
  const list = Array.from({ length: 20 }, (_, i) => i)

  const top = visibleWindow(list, 0)
  assert.strictEqual(top.start, 0)
  assert.strictEqual(top.hiddenAbove, 0)
  assert.strictEqual(top.hiddenBelow, 8)
  assert.strictEqual(top.visible.length, 12)
  assert.strictEqual(top.visible[0], 0)

  const mid = visibleWindow(list, 10)
  assert.strictEqual(mid.start, 4)
  assert.strictEqual(mid.hiddenAbove, 4)
  assert.strictEqual(mid.hiddenBelow, 4)
  assert.strictEqual(mid.visible[mid.visible.length - 1], 15)

  const bottom = visibleWindow(list, 19)
  assert.strictEqual(bottom.start, 8)
  assert.strictEqual(bottom.hiddenAbove, 8)
  assert.strictEqual(bottom.hiddenBelow, 0)
  assert.strictEqual(bottom.visible[bottom.visible.length - 1], 19)
})

await test('HP-2: window shows the full short list without hints', () => {
  const list5 = [1, 2, 3, 4, 5]
  const w = visibleWindow(list5, 4)
  assert.strictEqual(w.start, 0)
  assert.strictEqual(w.hiddenAbove, 0)
  assert.strictEqual(w.hiddenBelow, 0)
  assert.deepStrictEqual(w.visible, list5)
})

console.log('')
console.log(`✓ ${passed} passed, ${failed} failed`)
if (failed > 0) process.exit(1)