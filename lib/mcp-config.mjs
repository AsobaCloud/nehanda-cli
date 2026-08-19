/**
 * mcp-config.mjs
 *
 * Reads mcp.json and merges configured servers into settings.mcp_servers.
 *
 * Search order (first found wins, then merged in priority order):
 *   1. <project-root>/mcp.json       — project-local, highest priority
 *   2. ~/.config/nehanda/mcp.json    — user global
 *
 * Expected mcp.json schema (standard Anthropic/Claude Desktop format):
 * {
 *   "mcpServers": {
 *     "server-name": {
 *       "command": "npx",
 *       "args": ["-y", "@some/mcp-server"],
 *       "env": { "API_KEY": "..." }
 *     }
 *   }
 * }
 *
 * The resulting entries are mapped into settings.mcp_servers using the
 * same shape that lib/tools.mjs and lib/mcp.mjs already consume:
 * { [name]: { command, args, env } }
 */

import fs from 'node:fs'
import path from 'node:path'
import os from 'node:os'

const GLOBAL_MCP_PATH = path.join(os.homedir(), '.config', 'nehanda', 'mcp.json')

/**
 * Resolve candidate mcp.json paths for a given project root.
 * Returns them in priority order (highest first).
 */
function candidatePaths(projectRoot) {
  const candidates = []
  if (projectRoot) candidates.push(path.join(projectRoot, 'mcp.json'))
  candidates.push(GLOBAL_MCP_PATH)
  return candidates
}

/**
 * Parse a single mcp.json file. Returns { servers: {name: config}, path } or null.
 * Tolerates missing files and bad JSON gracefully.
 */
function parseMcpFile(filePath) {
  try {
    const raw = fs.readFileSync(filePath, 'utf8')
    const parsed = JSON.parse(raw)
    const servers = parsed?.mcpServers
    if (!servers || typeof servers !== 'object' || Array.isArray(servers)) return null
    // Normalise each server entry
    const normalised = {}
    for (const [name, cfg] of Object.entries(servers)) {
      if (!cfg?.command || typeof cfg.command !== 'string') continue // skip invalid
      normalised[name] = {
        command: cfg.command,
        args: Array.isArray(cfg.args) ? cfg.args : [],
        env: (cfg.env && typeof cfg.env === 'object' && !Array.isArray(cfg.env)) ? cfg.env : {},
      }
    }
    return { servers: normalised, path: filePath }
  } catch {
    return null
  }
}

/**
 * Load all mcp.json files for the given project root and return a merged
 * server map. Project-local entries override global ones for the same name.
 *
 * @param {string} projectRoot  - Absolute path to the project working directory
 * @returns {{ servers: Record<string, {command:string, args:string[], env:object}>, sources: string[] }}
 */
export function loadMcpConfig(projectRoot) {
  const candidates = candidatePaths(projectRoot)
  const layers = []

  for (const p of candidates) {
    const result = parseMcpFile(p)
    if (result) layers.push(result)
  }

  // Merge layers: first found (project-local) wins
  const merged = {}
  // Iterate in reverse so the highest-priority (project-local, index 0) overwrites
  for (let i = layers.length - 1; i >= 0; i--) {
    Object.assign(merged, layers[i].servers)
  }

  return {
    servers: merged,
    sources: layers.map(l => l.path),
  }
}

/**
 * Merge mcp.json servers into an existing settings object in-place.
 * Settings object must have (or will receive) a `mcp_servers` key.
 * Existing programmatic entries (e.g. set via /config) are preserved;
 * mcp.json entries for the same name are overwritten by the file.
 *
 * @param {object} settings     - Effective settings object (mutated in place)
 * @param {string} projectRoot  - Absolute path to the project working directory
 * @returns {string[]} List of mcp.json files that were loaded
 */
export function applyMcpConfig(settings, projectRoot) {
  const { servers, sources } = loadMcpConfig(projectRoot)
  if (!settings.mcp_servers || typeof settings.mcp_servers !== 'object') {
    settings.mcp_servers = {}
  }
  Object.assign(settings.mcp_servers, servers)
  return sources
}

/**
 * Return the path where a new global mcp.json should be written.
 */
export function globalMcpConfigPath() {
  return GLOBAL_MCP_PATH
}

/**
 * Write a new or updated global mcp.json.  Creates parent dirs as needed.
 *
 * @param {Record<string, {command:string, args:string[], env:object}>} servers
 */
export function writeGlobalMcpConfig(servers) {
  const dir = path.dirname(GLOBAL_MCP_PATH)
  fs.mkdirSync(dir, { recursive: true })
  const content = JSON.stringify({ mcpServers: servers }, null, 2)
  fs.writeFileSync(GLOBAL_MCP_PATH, content, 'utf8')
}

/**
 * Read the current global mcp.json server map (empty object if not present).
 */
export function readGlobalMcpConfig() {
  const result = parseMcpFile(GLOBAL_MCP_PATH)
  return result?.servers ?? {}
}
