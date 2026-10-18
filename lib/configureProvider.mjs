/**
 * Goose-style provider configure wizard (#8).
 * Single flow: provider → API key → host → keyed model discovery → persist.
 */

import {
  discoverOpenAiCompatModels,
  discoverOllamaModels,
} from './modelDiscovery.mjs'

/** @typedef {{ id: string, label: string, provider: string, defaultHost: string, needsKey: boolean, keySlot: string|null, discovery: 'openai'|'ollama' }} ProviderPreset */

/** @type {ProviderPreset[]} */
export const PROVIDER_PRESETS = [
  {
    id: 'nehanda',
    label: 'Nehanda Cloud',
    provider: 'nehanda',
    defaultHost: 'https://nehanda-ml.asoba.co/v1',
    needsKey: true,
    keySlot: 'nehanda',
    discovery: 'openai',
  },
  {
    id: 'openrouter',
    label: 'OpenRouter',
    provider: 'openai_compatible',
    defaultHost: 'https://openrouter.ai/api/v1',
    needsKey: true,
    keySlot: 'openai_compat',
    discovery: 'openai',
  },
  {
    id: 'openai_compatible',
    label: 'OpenAI-compatible',
    provider: 'openai_compatible',
    defaultHost: 'https://api.openai.com/v1',
    needsKey: true,
    keySlot: 'openai_compat',
    discovery: 'openai',
  },
  {
    id: 'zhipu',
    label: 'Zhipu (z.ai)',
    provider: 'zhipu',
    defaultHost: 'https://open.bigmodel.cn/api/paas/v4',
    needsKey: true,
    keySlot: 'zhipu',
    discovery: 'openai',
  },
  {
    id: 'ollama',
    label: 'Ollama (local / LAN)',
    provider: 'ollama',
    defaultHost: 'http://localhost:11434',
    needsKey: false,
    keySlot: null,
    discovery: 'ollama',
  },
  {
    id: 'lm_studio',
    label: 'LM Studio',
    provider: 'lm_studio_local',
    defaultHost: 'http://localhost:1234/v1',
    needsKey: false,
    keySlot: 'openai_compat',
    discovery: 'openai',
  },
]

export function normalizeOpenAiBaseUrl(host) {
  const h = String(host || '').trim().replace(/\/+$/, '')
  if (!h) return ''
  return h
}

/** Ollama tags API wants host without trailing /v1. */
export function normalizeOllamaHost(host) {
  return String(host || '').trim().replace(/\/+$/, '').replace(/\/v1$/i, '')
}

/** Persist endpoint for agents.json — OpenAI-compat style /v1 for Ollama. */
export function endpointForAgent(preset, host) {
  if (preset.discovery === 'ollama') {
    const base = normalizeOllamaHost(host)
    return base ? `${base}/v1` : ''
  }
  return normalizeOpenAiBaseUrl(host)
}

/**
 * Discover models for a preset using host + optional API key.
 * @param {ProviderPreset} preset
 * @param {{ host: string, apiKey?: string, timeout?: number }} opts
 */
export async function discoverModelsForPreset(preset, opts = {}) {
  const timeout = opts.timeout || 5000
  if (preset.discovery === 'ollama') {
    const host = normalizeOllamaHost(opts.host)
    return discoverOllamaModels(host, { timeout })
  }
  const host = normalizeOpenAiBaseUrl(opts.host)
  return discoverOpenAiCompatModels(host, { apiKey: opts.apiKey || '', timeout })
}

/**
 * Interactive configure wizard.
 *
 * @param {object} deps
 * @param {(q: string) => Promise<string>} deps.ask
 * @param {(title: string, items: object[]) => Promise<object|null>} deps.select
 * @param {(msg: string) => void} [deps.notify]
 * @param {(ctx: {preset, host, apiKey}) => Promise<{success, models?, error?}>} [deps.discover]
 * @param {(args: {slot: string, key: string}) => Promise<void>} [deps.saveKey]
 * @param {(preset: ProviderPreset) => Promise<string>} [deps.resolveExistingKey]
 * @param {(cfg: object) => Promise<void>} [deps.persist]
 * @param {{ presetId?: string, prefilledKey?: string }} [opts]
 * @returns {Promise<object|null>} configured result or null if cancelled
 */
export async function runConfigureWizard(deps, opts = {}) {
  const ask = deps.ask
  const select = deps.select
  const notify = deps.notify || (() => {})
  const discover = deps.discover || ((ctx) => discoverModelsForPreset(ctx.preset, ctx))
  const saveKey = deps.saveKey || (async () => {})
  const resolveExistingKey = deps.resolveExistingKey || (async () => '')
  const persist = deps.persist || (async () => {})

  let preset
  if (opts.presetId) {
    preset = PROVIDER_PRESETS.find(p => p.id === opts.presetId)
    if (!preset) throw new Error(`Unknown provider preset: ${opts.presetId}`)
  } else {
    const items = PROVIDER_PRESETS.map(p => ({
      id: p.id,
      label: p.label,
      desc: p.defaultHost,
      ...p,
    }))
    const picked = await select('Which provider?', items)
    if (!picked) return null
    preset = PROVIDER_PRESETS.find(p => p.id === picked.id) || picked
  }

  // ── API key ────────────────────────────────────────────────
  let apiKey = typeof opts.prefilledKey === 'string' ? opts.prefilledKey.trim() : ''
  let keyDirty = false

  if (preset.needsKey && !apiKey) {
    const existing = (await resolveExistingKey(preset)) || ''
    if (existing) {
      const choice = await select('Update API key?', [
        { id: 'keep', label: 'Keep existing key', desc: 'Use the key already in secure storage' },
        { id: 'replace', label: 'Replace key', desc: 'Enter a new API key' },
      ])
      if (!choice) return null
      if (choice.id === 'replace') {
        const entered = await ask(`${preset.label} API key:`)
        if (!entered) return null
        apiKey = entered.trim()
        keyDirty = true
      } else {
        apiKey = existing
      }
    } else {
      const entered = await ask(`${preset.label} API key:`)
      if (!entered) return null
      apiKey = entered.trim()
      keyDirty = true
    }
  } else if (preset.needsKey && apiKey && opts.prefilledKey) {
    // Key already saved by /key — do not re-save
    keyDirty = false
  }

  // ── Host ───────────────────────────────────────────────────
  const hostPrompt = preset.discovery === 'ollama'
    ? `Ollama host [${preset.defaultHost}]:`
    : `API host [${preset.defaultHost}]:`
  const hostAnswer = await ask(hostPrompt)
  const host = (hostAnswer && hostAnswer.trim()) || preset.defaultHost
  if (!host) return null

  const endpoint = endpointForAgent(preset, host)
  if (!endpoint) return null

  // ── Discovery ──────────────────────────────────────────────
  notify('Fetching models from provider…')
  let discovered = []
  let discoveryError = null
  try {
    const result = await discover({
      preset,
      host: preset.discovery === 'ollama' ? normalizeOllamaHost(host) : normalizeOpenAiBaseUrl(host),
      apiKey,
    })
    if (result?.success && Array.isArray(result.models)) {
      discovered = result.models
    } else {
      discoveryError = result?.error || 'Discovery failed'
      notify(`Discovery failed: ${discoveryError}`)
    }
  } catch (err) {
    discoveryError = err.message
    notify(`Discovery failed: ${discoveryError}`)
  }

  const modelItems = discovered.map(m => ({
    id: m.name,
    name: m.name,
    label: m.name,
    desc: 'Discovered',
    endpoint,
  }))
  modelItems.push({
    id: '__custom__',
    name: '__custom__',
    label: '(enter custom name…)',
    desc: discoveryError ? 'Discovery unavailable — type a model id' : 'Type a model id not listed above',
    endpoint,
  })

  const modelPick = await select('Select model', modelItems)
  if (!modelPick) return null

  let modelName = modelPick.name
  if (modelPick.id === '__custom__' || modelName === '__custom__') {
    const custom = await ask('Custom model name:')
    if (!custom || !custom.trim()) return null
    modelName = custom.trim()
  }

  if (keyDirty && preset.keySlot && apiKey) {
    await saveKey({ slot: preset.keySlot, key: apiKey })
  }

  const cfg = {
    presetId: preset.id,
    provider: preset.provider,
    model: modelName,
    endpoint,
    apiKey: preset.needsKey ? apiKey : (preset.provider === 'ollama' ? 'ollama' : apiKey),
    model_config: {
      provider: preset.provider,
      model_id: modelName,
      custom_model_name: modelName,
      base_url: endpoint,
      api_key: preset.provider === 'ollama' ? 'ollama' : apiKey,
    },
  }

  await persist(cfg)
  return cfg
}
