/**
 * energy-middleware.mjs
 *
 * ODSE energy normalization middleware for nehanda-cli.
 *
 * Intercepts MCP tool results containing energy production data and
 * transforms them through the `odse` Python package (ona-protocol)
 * before the model sees the payload.
 *
 * Flow:
 *   1. Detect OEM brand from MCP server name (e.g. "energy-huawei" → "huawei")
 *   2. If name doesn't resolve → fingerprint the payload fields
 *   3. If brand identified → spawn odse-transform.py, return normalized ODS-E JSON
 *   4. If no brand match → return payload unchanged (pass-through)
 *
 * Server naming convention (recommended in mcp.json):
 *   "energy-huawei", "energy-solaredge", "energy-sungrow", etc.
 *   Any prefix before the first hyphen-separated OEM key is stripped.
 *
 * No configuration required — drop a new energy MCP server into mcp.json
 * following the naming convention and normalization happens automatically.
 */

import { spawnSync } from 'node:child_process'
import path from 'node:path'
import { fileURLToPath } from 'node:url'

const __dirname = path.dirname(fileURLToPath(import.meta.url))
const TRANSFORM_SCRIPT = path.join(__dirname, 'scripts', 'odse-transform.py')
const TRANSFORM_TIMEOUT_MS = 30_000

// ─── All known ODSE source keys (from odse.transformer._get_transformer) ─────

const KNOWN_OEMS = new Set([
  'huawei', 'fusionsolar',
  'enphase', 'envoy',
  'solarman', 'logger',
  'solaredge',
  'fronius',
  'switch',
  'solaxcloud', 'solax',
  'fimer', 'auroravision',
  'sma',
  'solis', 'soliscloud',
  'sungrow', 'isolarcloud',
  'higeco',
  'eskom', 'eskom_portal', 'eskom_amr', 'nrs049',
  'sungrow_bess', 'powertitan',
  'byd', 'byd_bess',
  'vestas',
  'siemens_gamesa', 'sgre',
  'nordex',
  'terraco',
  'csv', 'generic_csv', 'generic',
])

// Canonical source key for known aliases — maps anything → the key odse accepts
const OEM_ALIASES = {
  fusionsolar: 'huawei',
  envoy: 'enphase',
  logger: 'solarman',
  auroravision: 'fimer',
  solax: 'solaxcloud',
  soliscloud: 'solis',
  isolarcloud: 'sungrow',
  eskom_portal: 'eskom',
  eskom_amr: 'eskom-amr',
  nrs049: 'eskom-amr',
  powertitan: 'sungrow-bess',
  byd_bess: 'byd-bess',
  sgre: 'siemens-gamesa',
  generic_csv: 'generic',
  csv: 'generic',
}

function canonicalOem(key) {
  const k = key.toLowerCase().replace(/-/g, '_')
  return OEM_ALIASES[k] || (KNOWN_OEMS.has(k.replace(/_/g, '-')) ? k.replace(/_/g, '-') : null)
    || (KNOWN_OEMS.has(k) ? k : null)
}

// ─── Name-based OEM detection ─────────────────────────────────────────────────

/**
 * Attempt to extract an OEM key from the MCP server name.
 * Handles patterns like:
 *   "energy-huawei"     → "huawei"
 *   "mcp-solaredge"     → "solaredge"
 *   "power-sungrow-v2"  → "sungrow"
 *   "huawei"            → "huawei"  (bare name)
 *
 * @param {string} serverName
 * @returns {string|null}  canonical OEM key or null
 */
export function detectOemFromServerName(serverName) {
  if (!serverName) return null
  const parts = serverName.toLowerCase().split(/[-_]/)
  // Try each segment (and pairs) as an OEM key, skipping generic prefixes
  const SKIP = new Set(['energy', 'mcp', 'power', 'solar', 'api', 'v1', 'v2', 'v3', 'prod', 'dev'])
  for (const part of parts) {
    if (SKIP.has(part)) continue
    const oem = canonicalOem(part)
    if (oem) return oem
  }
  return null
}

// ─── Payload fingerprinting ───────────────────────────────────────────────────

/**
 * Field-signature fingerprints derived from ona-protocol transform YAMLs.
 * Each entry: { oem: string, test: (parsed: any) → boolean }
 * Ordered from most-specific to least-specific.
 */
const FINGERPRINTS = [
  // Huawei FusionSolar CSV: has "inverter_state" or "run_state" columns
  {
    oem: 'huawei',
    test: p =>
      typeof p === 'string'
        ? /inverter.?state|run.?state|fusionsolar/i.test(p)
        : (Array.isArray(p) ? p[0] : p)?.inverter_state !== undefined,
  },
  // Enphase Envoy JSON array: { end_at, wh_del, devices_reporting }
  {
    oem: 'enphase',
    test: p => {
      const first = Array.isArray(p) ? p[0] : null
      return first && 'end_at' in first && 'wh_del' in first
    },
  },
  // SolarEdge: { data: { telemetries: [...] } } or { energy: { values: [...] } }
  {
    oem: 'solaredge',
    test: p =>
      p?.data?.telemetries !== undefined ||
      p?.data?.powerDetails !== undefined ||
      (typeof p === 'string' && /solaredge|L1Data|totalActivePower/i.test(p)),
  },
  // Fronius Solar API: { Head: { Timestamp }, Body: { Data: { Site } } }
  {
    oem: 'fronius',
    test: p => p?.Head?.Timestamp !== undefined && p?.Body?.Data !== undefined,
  },
  // Sungrow iSolarCloud: has stationCode or device_type fields
  {
    oem: 'sungrow',
    test: p =>
      p?.stationCode !== undefined ||
      p?.device_type !== undefined ||
      (typeof p === 'string' && /isolarcloud|sungrow/i.test(p)),
  },
  // SMA: { records: [{ normalized: { active_power_w, status_code } }] }
  {
    oem: 'sma',
    test: p => {
      const first = Array.isArray(p?.records) ? p.records[0] : null
      return first?.normalized?.active_power_w !== undefined && first?.normalized?.status_code !== undefined
    },
  },
  // SolaxCloud: { success, result: { acpower, inverterStatus } }
  {
    oem: 'solaxcloud',
    test: p =>
      p?.result?.acpower !== undefined ||
      p?.result?.inverterStatus !== undefined,
  },
  // Fimer AuroraVision: { series: [{ date, energy, unit }] }
  {
    oem: 'fimer',
    test: p => Array.isArray(p?.series) && p.series[0]?.energy !== undefined && p.series[0]?.date !== undefined,
  },
  // Solarman logger CSV: "Update Time" + "Generation(kWh)" columns
  {
    oem: 'solarman',
    test: p =>
      typeof p === 'string' && /Update.?Time|Generation.*kWh|Device.?State/i.test(p),
  },
  // Vestas: CSV with wind turbine fields
  {
    oem: 'vestas',
    test: p =>
      typeof p === 'string' && /rotor_rpm|nacelle_position|wind_speed.*turbine_state/i.test(p),
  },
  // Siemens Gamesa: CSV with metmast + bearing_temp
  {
    oem: 'siemens-gamesa',
    test: p =>
      typeof p === 'string' && /wind_speed_metmast|bearing_temp|availability_status/i.test(p),
  },
  // Nordex: CSV with blade_angle + transformer_temp
  {
    oem: 'nordex',
    test: p =>
      typeof p === 'string' && /blade_angle|transformer_temp|turbine_status/i.test(p),
  },
  // BYD BESS: has soc or state_of_charge
  {
    oem: 'byd-bess',
    test: p =>
      p?.soc !== undefined || p?.state_of_charge !== undefined ||
      (typeof p === 'string' && /state.?of.?charge|byd/i.test(p)),
  },
  // Terraco historian: { data: [{ timestamp, values: { "SITE.ActivePower": ... } }] }
  {
    oem: 'terraco',
    test: p => {
      const first = Array.isArray(p?.data) ? p.data[0] : null
      if (!first?.values) return false
      return Object.keys(first.values).some(k => /\.\w+/.test(k))
    },
  },
  // Higeco: records with connectionStatus + powerStatus
  {
    oem: 'higeco',
    test: p => {
      const first = Array.isArray(p?.records) ? p.records[0] : null
      return first?.normalized?.connectionStatus !== undefined
    },
  },
]

/**
 * Attempt to identify an OEM from the raw payload string via field fingerprinting.
 *
 * @param {string} content  raw MCP tool result content
 * @returns {string|null}   canonical OEM key or null
 */
export function detectOemFromPayload(content) {
  if (!content || typeof content !== 'string') return null

  // Try to parse as JSON for object-based fingerprints
  let parsed = null
  try { parsed = JSON.parse(content) } catch { /* not JSON — use raw string */ }

  const subject = parsed ?? content

  for (const { oem, test } of FINGERPRINTS) {
    try {
      if (test(subject)) return oem
    } catch { /* fingerprint threw — skip */ }
  }
  return null
}

// ─── Transform dispatch ───────────────────────────────────────────────────────

/**
 * Run the raw payload through odse-transform.py for the given OEM source.
 * Returns normalized ODS-E JSON string on success, null on failure (caller
 * should fall back to the original content).
 *
 * @param {string} source   ODSE source key
 * @param {string} payload  raw payload string
 * @param {object} opts     { assetId?, timezone? }
 * @returns {string|null}
 */
function runOdseTransform(source, payload, opts = {}) {
  const args = ['python3', TRANSFORM_SCRIPT, '--source', source]
  if (opts.assetId) args.push('--asset-id', opts.assetId)
  if (opts.timezone) args.push('--timezone', opts.timezone)

  const result = spawnSync(args[0], args.slice(1), {
    input: payload,
    encoding: 'utf8',
    timeout: TRANSFORM_TIMEOUT_MS,
    maxBuffer: 10 * 1024 * 1024,  // 10 MB
  })

  if (result.error) return null   // spawn failed (python3 not found, etc.)
  if (result.status === 2) return null   // import/usage error — log and skip
  if (result.status === 1) return null   // unknown source — pass through
  if (result.status !== 0) return null

  const out = (result.stdout || '').trim()
  if (!out) return null
  return out
}

// ─── Public API ───────────────────────────────────────────────────────────────

/**
 * Apply ODSE energy normalization to an MCP tool result.
 *
 * If the server name or payload fingerprint identifies a known OEM inverter
 * brand, the raw content is transformed to ODS-E normalized JSON.
 * Otherwise the content is returned unchanged.
 *
 * @param {string} serverName   MCP server name (from mcp__<server>__<tool>)
 * @param {string} content      raw tool result content string
 * @param {object} opts         optional hints: { assetId?, timezone? }
 * @returns {{ content: string, normalized: boolean, oem: string|null }}
 */
export async function applyEnergyNormalization(serverName, content, opts = {}) {
  if (!content || typeof content !== 'string') {
    return { content, normalized: false, oem: null }
  }

  // Step 1: try name-based detection
  let oem = detectOemFromServerName(serverName)

  // Step 2: fall back to payload fingerprinting
  if (!oem) oem = detectOemFromPayload(content)

  // Step 3: no match — pass through unchanged
  if (!oem) return { content, normalized: false, oem: null }

  // Step 4: run transform
  const normalized = runOdseTransform(oem, content, opts)
  if (!normalized) {
    // Transform failed — return original, don't break the model turn
    return { content, normalized: false, oem }
  }

  // Wrap with a header so the model knows this was normalized
  const header = `[ODS-E normalized · source: ${oem}]\n`
  return { content: header + normalized, normalized: true, oem }
}
