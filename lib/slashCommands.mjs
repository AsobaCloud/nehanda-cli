/**
 * Single source of truth for nehanda-ui slash commands + help (#9).
 * Autocomplete uses top-level entries (or nested when the user has typed a parent prefix).
 */

/** @typedef {{ name: string, desc: string, autocomplete?: boolean }} SlashEntry */

/** @type {SlashEntry[]} */
export const TUI_SLASH_COMMANDS = [
  { name: '/model', desc: 'Configure provider → key → host → discover → model' },
  { name: '/key', desc: 'Set Nehanda API key, then continue configure' },
  { name: '/key openrouter [VALUE]', desc: 'OpenRouter key → continue configure', autocomplete: false },
  { name: '/key openai [VALUE]', desc: 'OpenAI-compat key → continue configure', autocomplete: false },
  { name: '/key zhipu [VALUE]', desc: 'Zhipu (z.ai) key → continue configure', autocomplete: false },
  { name: '/key nehanda [VALUE]', desc: 'Nehanda key → continue configure', autocomplete: false },
  { name: '/config', desc: 'Show provider, model, endpoint, key status' },
  { name: '/mcp', desc: 'MCP server & tool management' },
  { name: '/mcp list', desc: 'List tools from configured MCP servers', autocomplete: false },
  { name: '/mcp status', desc: 'Show configured servers', autocomplete: false },
  { name: '/mcp reload [server]', desc: 'Reload mcp.json into this session', autocomplete: false },
  { name: '/mcp add <name> <command> [args…]', desc: 'Register a server (~/.config/nehanda/mcp.json)', autocomplete: false },
  { name: '/mcp env', desc: 'Show env vars for every server (values masked)', autocomplete: false },
  { name: '/mcp env <server>', desc: 'Show env vars for one server', autocomplete: false },
  { name: '/mcp env <server> <NAME> <VALUE>', desc: 'Set env var  e.g. /mcp env jupyter JUPYTER_TOKEN sk-…', autocomplete: false },
  { name: '/mcp env unset <server> <NAME>', desc: 'Remove an env var from a server', autocomplete: false },
  { name: '/clear', desc: 'New conversation' },
  { name: '/retry', desc: 'Resend the last failed message' },
  { name: '/help', desc: 'Show this help' },
  { name: '/exit', desc: 'Quit (/quit)' },
]

/** Top-level commands only (slash menu when typing `/`). */
export function topLevelSlashCommands() {
  return TUI_SLASH_COMMANDS.filter(c => c.autocomplete !== false && !c.name.slice(1).includes(' '))
}

/**
 * Filter for Ink slash autocomplete.
 * - Bare `/foo` → top-level matches
 * - `/mcp…` → MCP entries (including subcommands)
 * - `/key…` → /key variants
 */
export function filterSlashAutocomplete(filter) {
  const f = String(filter || '')
  const lower = f.toLowerCase()
  if (lower.startsWith('/mcp')) {
    const needle = lower.trimEnd()
    return TUI_SLASH_COMMANDS.filter(c => {
      if (!c.name.startsWith('/mcp')) return false
      if (needle === '/mcp') return true
      return c.name.toLowerCase().startsWith(needle)
    })
  }
  if (lower.startsWith('/key')) {
    const needle = lower.trimEnd()
    return TUI_SLASH_COMMANDS.filter(c => {
      if (!c.name.startsWith('/key')) return false
      if (needle === '/key') return true
      return c.name.toLowerCase().startsWith(needle)
    })
  }
  return topLevelSlashCommands().filter(c =>
    c.name.startsWith(f) || c.name.includes(f.slice(1)))
}

/** Full help text for `/help` (no chalk — caller may colorize). */
export function formatTuiHelpText() {
  const pad = Math.max(...TUI_SLASH_COMMANDS.map(c => c.name.length), 12) + 2
  const lines = ['', 'Commands:', '']
  let lastGroup = ''
  for (const c of TUI_SLASH_COMMANDS) {
    const group = c.name.split(/\s+/)[0]
    if (lastGroup && group !== lastGroup && (group === '/mcp' || lastGroup === '/mcp' || group === '/clear')) {
      lines.push('')
    }
    lastGroup = group
    lines.push(`  ${c.name.padEnd(pad)}${c.desc}`)
  }
  lines.push('')
  return lines.join('\n')
}

/**
 * Parse `/mcp env …` argument tokens (after `/mcp`).
 * @param {string[]} parts - e.g. ['env'] | ['env','jupyter'] | ['env','unset','jupyter','TOKEN'] | ['env','jupyter','TOKEN','sk-…']
 * @returns {{ action: string, server?: string, name?: string, value?: string, error?: string }}
 */
export function parseMcpEnvParts(parts) {
  // parts[0] should be 'env'
  const rest = parts.slice(1)
  if (rest.length === 0) return { action: 'list_all' }

  const head = (rest[0] || '').toLowerCase()
  if (head === 'unset' || head === 'clear') {
    const server = rest[1]
    const name = rest[2]
    if (!server || !name) {
      return {
        action: 'usage_error',
        error: 'Usage: /mcp env unset <server> <NAME>\n  Example: /mcp env unset jupyter JUPYTER_TOKEN',
      }
    }
    if (rest.length > 3) {
      return {
        action: 'usage_error',
        error: 'Usage: /mcp env unset <server> <NAME>\n  Do not pass a value when unsetting.',
      }
    }
    return { action: 'unset', server, name }
  }

  const server = rest[0]
  if (rest.length === 1) return { action: 'list_server', server }

  const name = rest[1]
  const valueParts = rest.slice(2)
  if (valueParts.length === 0) {
    return {
      action: 'usage_error',
      error: [
        `Missing VALUE for ${name}.`,
        `  Set:   /mcp env ${server} ${name} <VALUE>`,
        `  Unset: /mcp env unset ${server} ${name}`,
      ].join('\n'),
    }
  }
  return { action: 'set', server, name, value: valueParts.join(' ') }
}
