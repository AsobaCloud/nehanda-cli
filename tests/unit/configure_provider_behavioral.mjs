#!/usr/bin/env node
/**
 * Behavioral tests — Unified /model configure wizard with keyed discovery (#8)
 *
 * HP-1: OpenAI-compat discovery calls GET /v1/models with Bearer key
 * HP-2: Discovery 401 → failure result (caller falls back to custom name)
 * HP-3: Ollama discovery uses /api/tags without Authorization
 * HP-4: Empty discovery → wizard still accepts custom model name and persists
 * HP-5: Wizard with prefilled openrouter key continues host → discover → model
 * HP-6: Existing key offers keep; keep path does not overwrite secure storage
 * HP-7: supportsDiscovery is true for openai_compatible / zhipu / nehanda / lm_studio_local
 * HP-8: Embedding model ids are filtered from OpenAI-compat catalog
 */

import assert from 'node:assert'
import fs from 'node:fs'
import os from 'node:os'
import path from 'node:path'
import http from 'node:http'

const TMP_ONA = fs.mkdtempSync(path.join(os.tmpdir(), 'ona-configure-test-'))
const TMP_SECURE = path.join(TMP_ONA, 'secure')
fs.mkdirSync(TMP_SECURE, { recursive: true, mode: 0o700 })
process.env.ONAHOME = TMP_ONA

for (const v of ['OPENAI_API_KEY', 'OPENROUTER_API_KEY', 'ZAI_API_KEY', 'NEHANDA_API_KEY']) {
  delete process.env[v]
}

import {
  discoverOpenAiCompatModels,
  discoverOllamaModels,
} from '../../lib/modelDiscovery.mjs'
import { supportsDiscovery } from '../../lib/modelConfig.mjs'
import {
  runConfigureWizard,
  PROVIDER_PRESETS,
} from '../../lib/configureProvider.mjs'
import {
  saveOpenAiCompatKey,
  resolveOpenAiCompatKey,
} from '../../lib/auth.mjs'

let passed = 0
let failed = 0

function test(name, fn) {
  return Promise.resolve()
    .then(() => fn())
    .then(() => { console.log(`✓ ${name}`); passed++ })
    .catch(err => { console.error(`✗ ${name}\n  ${err.stack || err.message}`); failed++ })
}

function clearSecure() {
  try { fs.unlinkSync(path.join(TMP_SECURE, 'anthropic.json')) } catch { /* ok */ }
}

/** Local HTTP server that records the last request. */
function startMockServer(handler) {
  const server = http.createServer((req, res) => handler(req, res))
  return new Promise(resolve => {
    server.listen(0, '127.0.0.1', () => {
      const { port } = server.address()
      resolve({
        port,
        baseUrl: `http://127.0.0.1:${port}/v1`,
        ollamaBase: `http://127.0.0.1:${port}`,
        close: () => new Promise((r, j) => server.close(e => e ? j(e) : r())),
      })
    })
  })
}

console.log('Running configure-provider behavioral tests (#8)…\n')

// ── HP-1 ─────────────────────────────────────────────────────────────────────

await test('HP-1: OpenAI-compat discovery uses Bearer key against /v1/models', async () => {
  let seenAuth = null
  let seenPath = null
  const mock = await startMockServer((req, res) => {
    seenPath = req.url
    seenAuth = req.headers.authorization || ''
    res.writeHead(200, { 'Content-Type': 'application/json' })
    res.end(JSON.stringify({
      data: [
        { id: 'openai/gpt-4o' },
        { id: 'anthropic/claude-sonnet-4' },
      ],
    }))
  })
  try {
    const result = await discoverOpenAiCompatModels(mock.baseUrl, {
      apiKey: 'sk-or-test-key',
      timeout: 3000,
    })
    assert.strictEqual(result.success, true)
    assert.strictEqual(seenPath, '/v1/models')
    assert.strictEqual(seenAuth, 'Bearer sk-or-test-key')
    assert.deepStrictEqual(result.models.map(m => m.name), [
      'openai/gpt-4o',
      'anthropic/claude-sonnet-4',
    ])
  } finally {
    await mock.close()
  }
})

// ── HP-2 ─────────────────────────────────────────────────────────────────────

await test('HP-2: Discovery 401 returns failure (no models)', async () => {
  const mock = await startMockServer((req, res) => {
    res.writeHead(401, { 'Content-Type': 'application/json' })
    res.end(JSON.stringify({ error: 'unauthorized' }))
  })
  try {
    const result = await discoverOpenAiCompatModels(mock.baseUrl, {
      apiKey: 'sk-bad',
      timeout: 3000,
    })
    assert.strictEqual(result.success, false)
    assert.ok(result.error.includes('401') || /auth|unauthorized/i.test(result.error))
    assert.ok(!result.models || result.models.length === 0)
  } finally {
    await mock.close()
  }
})

// ── HP-3 ─────────────────────────────────────────────────────────────────────

await test('HP-3: Ollama discovery hits /api/tags without Authorization', async () => {
  let seenAuth = null
  let seenPath = null
  const mock = await startMockServer((req, res) => {
    seenPath = req.url
    seenAuth = req.headers.authorization
    res.writeHead(200, { 'Content-Type': 'application/json' })
    res.end(JSON.stringify({
      models: [{ name: 'deepseek-coder-v2:latest', size: 1000 }],
    }))
  })
  try {
    const result = await discoverOllamaModels(mock.ollamaBase, { timeout: 3000 })
    assert.strictEqual(result.success, true)
    assert.strictEqual(seenPath, '/api/tags')
    assert.strictEqual(seenAuth, undefined)
    assert.strictEqual(result.models[0].name, 'deepseek-coder-v2:latest')
  } finally {
    await mock.close()
  }
})

// ── HP-8 (discovery filter) ──────────────────────────────────────────────────

await test('HP-8: Embedding model ids filtered from OpenAI catalog', async () => {
  const mock = await startMockServer((req, res) => {
    res.writeHead(200, { 'Content-Type': 'application/json' })
    res.end(JSON.stringify({
      data: [
        { id: 'gpt-4o' },
        { id: 'text-embedding-3-small' },
        { id: 'openai/text-embedding-ada-002' },
      ],
    }))
  })
  try {
    const result = await discoverOpenAiCompatModels(mock.baseUrl, { apiKey: 'sk-x', timeout: 3000 })
    assert.strictEqual(result.success, true)
    assert.deepStrictEqual(result.models.map(m => m.name), ['gpt-4o'])
  } finally {
    await mock.close()
  }
})

// ── HP-7 ─────────────────────────────────────────────────────────────────────

await test('HP-7: supportsDiscovery true for keyed OpenAI-compat providers', () => {
  assert.strictEqual(supportsDiscovery('openai_compatible'), true)
  assert.strictEqual(supportsDiscovery('zhipu'), true)
  assert.strictEqual(supportsDiscovery('nehanda'), true)
  assert.strictEqual(supportsDiscovery('lm_studio_local'), true)
  assert.strictEqual(supportsDiscovery('ollama'), true)
})

// ── HP-4 ─────────────────────────────────────────────────────────────────────

await test('HP-4: Failed discovery still persists custom model name', async () => {
  clearSecure()
  const persisted = []
  const keySaves = []

  const result = await runConfigureWizard({
    ask: async (q) => {
      if (/host/i.test(q)) return 'https://openrouter.ai/api/v1'
      if (/custom|model name/i.test(q)) return 'my/custom-model'
      if (/api key/i.test(q)) return 'sk-or-fresh'
      return ''
    },
    select: async (title, items) => {
      if (/provider/i.test(title)) {
        return items.find(i => i.id === 'openrouter')
      }
      // Model picker: only custom entry when discovery fails
      if (/model/i.test(title)) {
        return items.find(i => i.id === '__custom__') || items[items.length - 1]
      }
      return items[0]
    },
    notify: () => {},
    discover: async () => ({ success: false, error: 'timeout', models: [] }),
    saveKey: async ({ slot, key }) => { keySaves.push({ slot, key }) },
    resolveExistingKey: async () => '',
    persist: async (cfg) => { persisted.push(cfg) },
  })

  assert.ok(result)
  assert.strictEqual(result.model, 'my/custom-model')
  assert.strictEqual(result.endpoint, 'https://openrouter.ai/api/v1')
  assert.strictEqual(result.provider, 'openai_compatible')
  assert.strictEqual(persisted.length, 1)
  assert.strictEqual(persisted[0].model, 'my/custom-model')
  assert.strictEqual(keySaves.length, 1)
  assert.strictEqual(keySaves[0].key, 'sk-or-fresh')
})

// ── HP-5 ─────────────────────────────────────────────────────────────────────

await test('HP-5: Prefill openrouter key skips key prompt and discovers then persists', async () => {
  clearSecure()
  const persisted = []
  const asks = []

  const result = await runConfigureWizard({
    ask: async (q) => {
      asks.push(q)
      if (/host/i.test(q)) return 'https://openrouter.ai/api/v1'
      return ''
    },
    select: async (title, items) => {
      if (/provider/i.test(title)) return items.find(i => i.id === 'openrouter')
      if (/model/i.test(title)) return items.find(i => i.name === 'openai/gpt-4o') || items[0]
      return items[0]
    },
    notify: () => {},
    discover: async ({ apiKey, host }) => {
      assert.strictEqual(apiKey, 'sk-or-prefilled')
      assert.ok(host.includes('openrouter'))
      return {
        success: true,
        models: [{ name: 'openai/gpt-4o' }, { name: 'anthropic/claude-sonnet-4' }],
      }
    },
    saveKey: async () => {},
    resolveExistingKey: async () => '',
    persist: async (cfg) => { persisted.push(cfg) },
  }, { presetId: 'openrouter', prefilledKey: 'sk-or-prefilled' })

  assert.ok(result)
  assert.strictEqual(result.model, 'openai/gpt-4o')
  assert.ok(!asks.some(q => /api key/i.test(q)), 'must not re-ask for API key when prefilled')
  assert.strictEqual(persisted[0].model, 'openai/gpt-4o')
  assert.strictEqual(persisted[0].endpoint, 'https://openrouter.ai/api/v1')
})

// ── HP-6 ─────────────────────────────────────────────────────────────────────

await test('HP-6: Keep existing key does not call saveKey', async () => {
  clearSecure()
  saveOpenAiCompatKey('sk-already-there')
  const keySaves = []

  await runConfigureWizard({
    ask: async (q) => {
      if (/host/i.test(q)) return 'https://openrouter.ai/api/v1'
      return ''
    },
    select: async (title, items) => {
      if (/provider/i.test(title)) return items.find(i => i.id === 'openrouter')
      if (/update.*key|api key/i.test(title)) {
        return items.find(i => i.id === 'keep') || items[0]
      }
      if (/model/i.test(title)) return items.find(i => i.name === 'openai/gpt-4o') || items[0]
      return items[0]
    },
    notify: () => {},
    discover: async ({ apiKey }) => {
      assert.strictEqual(apiKey, 'sk-already-there')
      return { success: true, models: [{ name: 'openai/gpt-4o' }] }
    },
    saveKey: async (x) => { keySaves.push(x) },
    resolveExistingKey: async () => resolveOpenAiCompatKey(),
    persist: async () => {},
  })

  assert.strictEqual(keySaves.length, 0, 'keep path must not overwrite key')
})

assert.ok(PROVIDER_PRESETS.some(p => p.id === 'openrouter'), 'openrouter preset required')
assert.ok(PROVIDER_PRESETS.some(p => p.id === 'ollama'), 'ollama preset required')

console.log(`\n${passed} passed, ${failed} failed`)
if (failed) process.exit(1)
