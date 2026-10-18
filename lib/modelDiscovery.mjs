/**
 * Model Discovery Service
 * Queries Ollama (/api/tags) and OpenAI-compatible (/v1/models) endpoints.
 */

import http from 'node:http'
import https from 'node:https'
import { execSync } from 'node:child_process'

// ── Runtime Health Monitor ────────────────────────────────────

/**
 * Probes active inference endpoints and the Laya sidecar process to give the
 * TUI a live picture of backend health before each turn.
 *
 * Design follows the Kubernetes readiness-probe pattern: lightweight HEAD
 * requests with a hard 1-second timeout, so a crashed local Ollama instance
 * surfaces immediately rather than stalling the REPL.
 */
export class RuntimeHealthMonitor {
  constructor(config = {}) {
    this.activeModel = config.defaultModel || 'nehanda-rag-synthesis-27b'
    this.sidecarScript = config.sidecarScript || 'lib/scripts/laya_worker.py'
  }

  /**
   * Non-blocking HEAD probe of a local inference endpoint.
   * Cloud-managed endpoints (no URL or `cloud:` scheme) are assumed ONLINE
   * with zero latency — they have their own availability guarantees.
   */
  async probeEndpoint(targetUrl) {
    if (!targetUrl || targetUrl.startsWith('cloud:')) {
      return { status: 'ONLINE', latencyMs: 0, isLocal: false }
    }

    const start = Date.now()
    try {
      const url = new URL(targetUrl)
      const reqModule = url.protocol === 'https:' ? https : http

      return await new Promise((resolve) => {
        const req = reqModule.request(url, { method: 'HEAD', timeout: 1000 }, (res) => {
          resolve({
            status: res.statusCode < 400 ? 'ONLINE' : 'DEGRADED',
            latencyMs: Date.now() - start,
            isLocal: true,
          })
        })
        req.on('error', () => resolve({ status: 'OFFLINE', latencyMs: -1, isLocal: true }))
        req.on('timeout', () => {
          req.destroy()
          resolve({ status: 'TIMEOUT', latencyMs: -1, isLocal: true })
        })
        req.end()
      })
    } catch {
      return { status: 'INVALID_URL', latencyMs: -1, isLocal: true }
    }
  }

  /**
   * Checks liveness of the local Laya sidecar worker process via pgrep.
   * Returns { active: true, pid: string } or { active: false, pid: null }.
   */
  checkSidecarHealth() {
    try {
      const pidStr = execSync(`pgrep -f "${this.sidecarScript}"`, { encoding: 'utf8' }).trim()
      if (pidStr) {
        return { active: true, pid: pidStr.split('\n')[0] }
      }
    } catch {
      // pgrep exits 1 when no match — not an error
    }
    return { active: false, pid: null }
  }

  /**
   * Assembles a full runtime status telemetry payload for the UI renderer.
   * Runs endpoint probe and sidecar check concurrently.
   */
  async getRuntimeStatus(endpointUrl, usedTokens = 0, maxTokens = 128000) {
    const [endpointHealth, sidecarHealth] = await Promise.all([
      this.probeEndpoint(endpointUrl),
      Promise.resolve(this.checkSidecarHealth()),
    ])

    const ctxPercentage = maxTokens > 0 ? Math.round((usedTokens / maxTokens) * 100) : 0

    return {
      model: this.activeModel,
      endpoint: endpointUrl || 'Cloud Managed API',
      endpointStatus: endpointHealth.status,
      latency: endpointHealth.latencyMs,
      isLocal: endpointHealth.isLocal,
      sidecarStatus: sidecarHealth.active
        ? `ACTIVE (PID ${sidecarHealth.pid})`
        : 'INACTIVE',
      contextUsage: `${usedTokens}/${maxTokens} (${ctxPercentage}%)`,
    }
  }
}

/**
 * Discover available models from an Ollama server
 * @param {string} baseUrl - Base URL of the Ollama server (e.g., "http://localhost:11434")
 * @param {Object} options - Discovery options
 * @param {number} options.timeout - Request timeout in milliseconds (default: 5000)
 * @returns {Promise<DiscoveryResult>} Discovery result with models or error
 */
export async function discoverOllamaModels(baseUrl, options = {}) {
  const timeout = options.timeout || 5000
  
  try {
    // Construct discovery URL - remove trailing slashes and append endpoint
    const cleanBaseUrl = baseUrl.replace(/\/+$/, '')
    const discoveryUrl = `${cleanBaseUrl}/api/tags`
    
    // Make HTTP GET request with timeout
    const controller = new AbortController()
    const timeoutId = setTimeout(() => controller.abort(), timeout)
    
    let response
    try {
      response = await fetch(discoveryUrl, {
        method: 'GET',
        signal: controller.signal,
        headers: {
          'Accept': 'application/json'
        }
      })
    } finally {
      clearTimeout(timeoutId)
    }
    
    // Handle HTTP errors
    if (!response.ok) {
      const statusText = response.statusText || 'Unknown error'
      if (response.status === 404) {
        return {
          success: false,
          error: `Endpoint not found (404). The Ollama server at ${baseUrl} may not support model discovery.`
        }
      } else if (response.status >= 500) {
        return {
          success: false,
          error: `Server error (${response.status}): ${statusText}. The Ollama server encountered an internal error.`
        }
      } else {
        return {
          success: false,
          error: `HTTP error (${response.status}): ${statusText}`
        }
      }
    }
    
    // Parse JSON response
    let data
    try {
      data = await response.json()
    } catch (parseError) {
      return {
        success: false,
        error: `Invalid JSON response from server: ${parseError.message}`
      }
    }
    
    // Validate response structure
    if (!data || typeof data !== 'object') {
      return {
        success: false,
        error: 'Invalid response format: expected JSON object'
      }
    }
    
    if (!Array.isArray(data.models)) {
      return {
        success: false,
        error: 'Invalid response format: missing or invalid "models" array'
      }
    }
    
    // Extract and validate model information
    const models = []
    for (const model of data.models) {
      // Skip invalid model entries
      if (!model || typeof model !== 'object') {
        continue
      }
      
      // Require non-empty name field
      if (!model.name || typeof model.name !== 'string' || !model.name.trim()) {
        continue
      }
      
      // Extract model information
      models.push({
        name: model.name,
        modified_at: model.modified_at || null,
        size: typeof model.size === 'number' ? model.size : 0,
        digest: model.digest || null,
        details: model.details || null
      })
    }
    
    return {
      success: true,
      models
    }
    
  } catch (error) {
    // Handle connection errors and timeouts
    if (error.name === 'AbortError') {
      return {
        success: false,
        error: `Request timed out after ${timeout}ms. The Ollama server may be slow or unreachable.`
      }
    }
    
    // Handle network/connection errors
    if (error.code === 'ECONNREFUSED' || error.message.includes('fetch failed')) {
      return {
        success: false,
        error: `Cannot connect to Ollama server at ${baseUrl}. Make sure Ollama is running and accessible.`
      }
    }
    
    // Generic error
    return {
      success: false,
      error: `Discovery failed: ${error.message}`
    }
  }
}

/**
 * Normalize an OpenAI-compatible base URL so `{base}/models` resolves correctly.
 * Accepts either `https://host/v1` or `https://host`.
 */
export function normalizeOpenAiCompatBase(baseUrl) {
  if (!baseUrl || typeof baseUrl !== 'string') return ''
  // Do not force /v1 — Zhipu and similar hosts use paths like /api/paas/v4.
  return baseUrl.trim().replace(/\/+$/, '')
}

/**
 * Strip trailing /v1 so Ollama `/api/tags` is hit on the root host.
 */
export function normalizeOllamaBase(baseUrl) {
  if (!baseUrl || typeof baseUrl !== 'string') return ''
  return baseUrl.trim().replace(/\/+$/, '').replace(/\/v1$/i, '')
}

function isEmbeddingModelId(id) {
  return /embedding|whisper|tts|dall-e|moderation/i.test(id)
}

/**
 * Discover models from an OpenAI-compatible `/v1/models` endpoint (keyed).
 * @param {string} baseUrl - e.g. https://openrouter.ai/api/v1
 * @param {Object} options
 * @param {string} [options.apiKey] - Bearer token (optional for local servers)
 * @param {number} [options.timeout=5000]
 * @returns {Promise<{success:boolean, models?:Array<{name:string}>, error?:string}>}
 */
export async function discoverOpenAiCompatModels(baseUrl, options = {}) {
  const timeout = options.timeout || 5000
  const apiKey = typeof options.apiKey === 'string' ? options.apiKey.trim() : ''
  const cleanBase = normalizeOpenAiCompatBase(baseUrl)
  if (!cleanBase) {
    return { success: false, error: 'Base URL is required for model discovery.' }
  }
  const discoveryUrl = `${cleanBase}/models`

  try {
    const controller = new AbortController()
    const timeoutId = setTimeout(() => controller.abort(), timeout)
    let response
    try {
      const headers = { Accept: 'application/json' }
      if (apiKey) headers.Authorization = `Bearer ${apiKey}`
      response = await fetch(discoveryUrl, { method: 'GET', signal: controller.signal, headers })
    } finally {
      clearTimeout(timeoutId)
    }

    if (!response.ok) {
      const statusText = response.statusText || 'Unknown error'
      if (response.status === 401 || response.status === 403) {
        return {
          success: false,
          error: `Authentication failed (${response.status}): ${statusText}. Check the API key.`,
        }
      }
      return {
        success: false,
        error: `HTTP error (${response.status}): ${statusText}`,
      }
    }

    let data
    try {
      data = await response.json()
    } catch (parseError) {
      return { success: false, error: `Invalid JSON response from server: ${parseError.message}` }
    }

    const rows = Array.isArray(data?.data) ? data.data
      : Array.isArray(data?.models) ? data.models
      : null
    if (!rows) {
      return { success: false, error: 'Invalid response format: expected OpenAI-style { data: [...] }' }
    }

    const models = []
    const seen = new Set()
    for (const row of rows) {
      const id = typeof row?.id === 'string' ? row.id.trim()
        : typeof row?.name === 'string' ? row.name.trim()
        : ''
      if (!id || isEmbeddingModelId(id) || seen.has(id)) continue
      seen.add(id)
      models.push({ name: id })
    }

    return { success: true, models }
  } catch (error) {
    if (error.name === 'AbortError') {
      return {
        success: false,
        error: `Request timed out after ${timeout}ms. The server may be slow or unreachable.`,
      }
    }
    if (error.code === 'ECONNREFUSED' || (error.message && error.message.includes('fetch failed'))) {
      return {
        success: false,
        error: `Cannot connect to ${cleanBase}. Check the host and network.`,
      }
    }
    return { success: false, error: `Discovery failed: ${error.message}` }
  }
}

/**
 * Route discovery by provider id (used by bin/agent.mjs `/models`).
 * @param {string} provider
 * @param {{ baseUrl: string, apiKey?: string, timeout?: number }} opts
 */
export async function discoverModelsForProvider(provider, opts = {}) {
  const timeout = opts.timeout || 5000
  if (provider === 'ollama') {
    return discoverOllamaModels(normalizeOllamaBase(opts.baseUrl), { timeout })
  }
  return discoverOpenAiCompatModels(opts.baseUrl, {
    apiKey: opts.apiKey || '',
    timeout,
  })
}

/**
 * Format a list of models for display
 * @param {Array} models - Array of model objects from discovery
 * @param {Object} options - Formatting options
 * @param {string} options.sortBy - Sort by 'date', 'name', or 'size' (default: 'date')
 * @param {boolean} options.verbose - Include detailed information (default: false)
 * @returns {string} Formatted model list
 */
export function formatModelList(models, options = {}) {
  const sortBy = options.sortBy || 'date'
  const verbose = options.verbose || false
  
  if (!models || models.length === 0) {
    return 'No models found on the server.\n\nTo pull a model, run: ollama pull <model-name>'
  }
  
  // Sort models
  const sorted = [...models]
  if (sortBy === 'name') {
    sorted.sort((a, b) => a.name.localeCompare(b.name))
  } else if (sortBy === 'size') {
    sorted.sort((a, b) => b.size - a.size)
  } else {
    // Sort by modification date (newest first)
    sorted.sort((a, b) => {
      if (!a.modified_at) return 1
      if (!b.modified_at) return -1
      return new Date(b.modified_at) - new Date(a.modified_at)
    })
  }
  
  // Format output
  const lines = []
  lines.push('Available models:\n')
  
  for (const model of sorted) {
    const size = formatBytes(model.size)
    
    if (verbose && model.details) {
      const paramSize = model.details.parameter_size || 'unknown'
      const quantization = model.details.quantization_level || 'unknown'
      lines.push(`  ${model.name}`)
      lines.push(`    Size: ${size}`)
      lines.push(`    Parameters: ${paramSize}`)
      lines.push(`    Quantization: ${quantization}`)
    } else {
      lines.push(`  ${model.name.padEnd(40)} ${size}`)
    }
  }
  
  lines.push('\nUsage: /model ollama/<model-name>')
  lines.push('Example: /model ollama/llama3')
  
  return lines.join('\n')
}

/**
 * Format bytes in human-readable format
 * @param {number} bytes - Size in bytes
 * @returns {string} Formatted size string
 */
function formatBytes(bytes) {
  if (bytes === 0) return '0 B'
  if (bytes < 1024) return `${bytes} B`
  if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KB`
  if (bytes < 1024 * 1024 * 1024) return `${(bytes / (1024 * 1024)).toFixed(1)} MB`
  return `${(bytes / (1024 * 1024 * 1024)).toFixed(1)} GB`
}
