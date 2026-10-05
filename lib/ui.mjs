// Terminal UI — colors, formatting, spinner, markdown rendering
// Reference: claude-code components/Spinner.tsx, components/Markdown.tsx, components/PromptInput/
import chalk from 'chalk'
import { marked } from 'marked'
import TerminalRenderer from 'marked-terminal'
import { createRequire } from 'node:module'

// cli-table3 is a CommonJS module; load it via createRequire so this ESM file works.
const _require = createRequire(import.meta.url)
let Table
try {
  Table = _require('cli-table3')
} catch {
  Table = null // graceful degradation when not yet installed
}

// Configure marked for terminal output
marked.setOptions({
  renderer: new TerminalRenderer({
    code: chalk.yellow,
    codespan: chalk.yellow,
    strong: chalk.bold,
    em: chalk.italic,
    heading: chalk.bold.cyan,
    hr: () => chalk.dim('─'.repeat(60)),
    listitem: text => `  ${chalk.dim('•')} ${text}`,
    paragraph: text => text + '\n',
    table: chalk.reset,
    link: (href, title, text) => `${text} ${chalk.dim.underline(href)}`,
  })
})

export function renderMarkdown(text) {
  if (!text) return ''
  try {
    return marked(text).replace(/\n{3,}/g, '\n\n').trimEnd()
  } catch {
    return text
  }
}

// ── Colors ──────────────────────────────────────────────────

export const colors = {
  banner: chalk.bold.hex('#cc785c'),       // warm bronze for ona branding
  version: chalk.dim,
  provider: chalk.cyan,
  model: chalk.bold.white,
  endpoint: chalk.dim,
  prompt: chalk.bold.hex('#cc785c'),       // matches banner
  promptArrow: chalk.bold.hex('#cc785c'),
  assistant: chalk.reset,
  toolName: chalk.bold.yellow,
  toolLabel: chalk.dim.yellow,
  toolResult: chalk.dim,
  toolError: chalk.red,
  error: chalk.red,
  success: chalk.green,
  dim: chalk.dim,
  info: chalk.blue,
  warning: chalk.yellow,
  command: chalk.cyan,
  key: chalk.dim,
  value: chalk.white,
  header: chalk.bold.underline,
  separator: chalk.dim,
}

// ── Banner ──────────────────────────────────────────────────

export function printBanner(version, dbPath, bare) {
  const inner = `   ona v${version}   `
  const width = inner.length
  const lines = []
  lines.push('')
  lines.push(colors.banner(`  ╭${'─'.repeat(width)}╮`))
  lines.push(colors.banner(`  │`) + `${inner}` + colors.banner(`│`))
  lines.push(colors.banner(`  ╰${'─'.repeat(width)}╯`))
  if (bare) lines.push(colors.dim(`  [bare mode]`))
  lines.push(colors.dim(`  DB: ${dbPath}`))
  lines.push('')
  return lines.join('\n')
}

export function printProviderBanner(provider, wireModel, endpoint) {
  const lines = []
  lines.push(`  ${colors.key('Provider:')} ${colors.provider(provider)}`)
  lines.push(`  ${colors.key('Model:')}    ${colors.model(wireModel)}`)
  lines.push(`  ${colors.key('Endpoint:')} ${colors.endpoint(endpoint)}`)
  lines.push('')
  return lines.join('\n')
}

// ── Prompt ──────────────────────────────────────────────────

export function formatPrompt() {
  return colors.promptArrow('❯ ')
}

// ── Help ────────────────────────────────────────────────────

export function formatHelp(provider, dbPath) {
  const cmd = (name, desc) => `  ${colors.command(name.padEnd(18))} ${colors.dim(desc)}`
  const lines = [
    '',
    colors.header('Commands'),
    cmd('/phase', 'Show current SDLC phase'),
    cmd('/plan', 'Show plan status'),
    cmd('/code', 'Implement approved plan'),
    cmd('/test', 'Generate and run tests'),
    cmd('/verify', 'Coverage report'),
    cmd('/done', 'Complete workflow'),
    '',
    cmd('/init', 'Create Ona.md'),
    cmd('/diff', 'Uncommitted changes'),
    cmd('/cost', 'Token usage and cost'),
    cmd('/doctor', 'Environment diagnostics'),
    cmd('/permissions', 'Permission rules'),
    cmd('/pr-comments', 'PR comments (requires gh)'),
    cmd('/compact', 'Compact conversation'),
    cmd('/team', 'Manage teams'),
    '',
    cmd('/model [name]', 'Change model'),
    cmd('/login', 'Store credentials'),
    cmd('/logout', 'Clear credentials'),
    cmd('/status', 'Auth status'),
    cmd('/config', 'Show / set settings'),
    cmd('/config set <key> <value>', 'Set any config value'),
    cmd('/mcp', 'MCP server management & guide'),
    cmd('/mcp list', 'List all discovered tools & descriptions'),
    cmd('/mcp status', 'Show configured MCP servers'),
    cmd('/mcp env [server] [KEY] [val]', 'Set MCP env vars (API keys)'),
    cmd('/clear', 'New conversation'),
    cmd('/exit', 'Quit (/quit)'),
    '',
    `  ${colors.key('Provider:')} ${colors.provider(provider)}`,
    `  ${colors.key('DB:')}       ${colors.dim(dbPath)}`,
    '',
  ]
  return lines.join('\n')
}

export function formatMcpHelp() {
  const cmd = (name, desc) => `  ${colors.command(name.padEnd(28))} ${colors.dim(desc)}`
  const lines = [
    '',
    colors.header('MCP Server & Tool Commands'),
    cmd('/mcp list', 'Discover and list all available MCP tools'),
    cmd('/mcp status', 'Show configured servers and command paths'),
    cmd('/mcp reload [server]', 'Reload mcp.json configs into session'),
    cmd('/mcp add <name> <cmd> [args]', 'Register a new MCP server'),
    cmd('/mcp env [server] [KEY] [val]', 'View or set server environment variables'),
    '',
    colors.dim('  Tip: Run /mcp list to inspect tools injected into the assistant.'),
    '',
  ]
  return lines.join('\n')
}

// ── Tool display ────────────────────────────────────────────

export function formatToolStart(toolName) {
  return colors.toolLabel('  ┌ ') + colors.toolName(toolName)
}

export function formatToolResult(toolName, content, isError) {
  const icon = isError ? colors.toolError('✗') : colors.success('✓')
  const label = colors.toolLabel('  └ ')

  // Attempt JSON-array table rendering when not an error and cli-table3 is available.
  if (!isError && Table && typeof content === 'string' && content.trimStart().startsWith('[')) {
    try {
      const parsed = JSON.parse(content)
      if (Array.isArray(parsed) && parsed.length > 0 && typeof parsed[0] === 'object' && parsed[0] !== null) {
        const TABLE_ROW_CAP = 10
        const rows = parsed.slice(0, TABLE_ROW_CAP)
        const headers = Object.keys(rows[0])
        const termWidth = process.stdout.columns || 80
        // Distribute column widths evenly, leaving room for borders (3 chars per col).
        const colWidth = Math.max(8, Math.floor((termWidth - headers.length * 3 - 1) / headers.length))
        const table = new Table({
          head: headers.map(h => colors.toolName(String(h))),
          colWidths: headers.map(() => colWidth),
          wordWrap: true,
          style: { head: [], border: ['dim'] },
        })
        for (const row of rows) {
          table.push(headers.map(h => {
            const v = row[h]
            return v === null || v === undefined ? '' : String(v)
          }))
        }
        const truncNote = parsed.length > TABLE_ROW_CAP
          ? colors.dim(`\n  … ${parsed.length - TABLE_ROW_CAP} more rows (${parsed.length} total)`)
          : ''
        const rowCountLine = colors.dim(`  ${parsed.length} row${parsed.length === 1 ? '' : 's'} — ${toolName}`)
        return label + icon + '\n' + table.toString() + truncNote + '\n' + rowCountLine
      }
    } catch {
      // Not valid JSON — fall through to plain preview
    }
  }

  const preview = (content || '').split('\n')[0].slice(0, 80)
  return label + icon + ' ' + colors.dim(preview)
}

// ── Spinner ─────────────────────────────────────────────────

const SPINNER_FRAMES = ['⠋', '⠙', '⠹', '⠸', '⠼', '⠴', '⠦', '⠧', '⠇', '⠏']

export class Spinner {
  constructor(io) {
    this.io = io
    this.frame = 0
    this.interval = null
    this.active = false
    this.message = 'Thinking'
  }

  start(message) {
    if (this.active) return
    this.active = true
    this.message = message || 'Thinking'
    this.frame = 0
    this.interval = setInterval(() => {
      const f = SPINNER_FRAMES[this.frame % SPINNER_FRAMES.length]
      this.io.write(`\r${colors.dim(f + ' ' + this.message + '...')}`)
      this.frame++
    }, 80)
  }

  stop() {
    if (!this.active) return
    this.active = false
    if (this.interval) {
      clearInterval(this.interval)
      this.interval = null
    }
    this.io.write('\r' + ' '.repeat(this.message.length + 20) + '\r')
  }
}

// ── Model change ────────────────────────────────────────────

export function formatModelChange(provider, modelId, wireModel, isCustom = false) {
  const customSuffix = isCustom ? colors.dim(' (custom)') : ''
  return `  ${colors.success('✓')} Model: ${colors.provider(provider)} ${colors.dim('/')} ${colors.model(wireModel)}${customSuffix}`
}

// ── Status ──────────────────────────────────────────────────

export function formatStatus(status) {
  const lines = ['', colors.header('Auth Status')]
  for (const [k, v] of Object.entries(status)) {
    if (k === 'alsoConfigured') {
      const ac = v || {}
      if (ac.ignoredHints?.length) {
        lines.push(`  ${colors.key('Notes:')}`)
        for (const hint of ac.ignoredHints) lines.push(`    ${colors.dim(hint)}`)
      }
      continue
    }
    const display = typeof v === 'object' ? JSON.stringify(v) : String(v)
    lines.push(`  ${colors.key(k + ':')} ${colors.value(display)}`)
  }
  lines.push('')
  return lines.join('\n')
}

// ── Config ──────────────────────────────────────────────────

export function formatConfig(settings) {
  const lines = ['', colors.header('Settings')]
  
  // Show model config with custom indicator
  if (settings.model_config) {
    const { provider, model_id, custom_model_name, base_url } = settings.model_config
    lines.push(`  ${colors.key('Provider:')} ${colors.provider(provider)}`)
    
    if (custom_model_name) {
      lines.push(`  ${colors.key('Model:')}    ${colors.model(custom_model_name)} ${colors.dim('(custom)')}`)
    } else {
      lines.push(`  ${colors.key('Model:')}    ${colors.model(model_id)}`)
    }
    
    if (base_url) {
      lines.push(`  ${colors.key('Base URL:')} ${colors.dim(base_url)}`)
    }
    lines.push('')
  }
  
  // Show full JSON for other settings
  const json = JSON.stringify(settings, null, 2)
  for (const line of json.split('\n')) {
    lines.push('  ' + colors.dim(line))
  }
  lines.push('')
  return lines.join('\n')
}

// ── Separator ───────────────────────────────────────────────

export function separator() {
  return colors.separator('─'.repeat(60))
}

// ── Session summary ──────────────────────────────────────────

export function formatSessionSummary(sessionId, tokens, model, provider) {
  const { input = 0, output = 0, calls = 0 } = tokens
  const total = input + output
  const sep = colors.dim('─'.repeat(48))

  const lines = [
    '',
    sep,
    `  ${colors.header('Session Summary')}`,
    `  ${colors.key('Session:')}  ${colors.dim(sessionId)}`,
    `  ${colors.key('Model:')}    ${colors.provider(provider)} ${colors.dim('/')} ${colors.model(model)}`,
    `  ${colors.key('API calls:')} ${colors.value(String(calls))}`,
    '',
    `  ${colors.header('Token Usage')}`,
    `  ${colors.key('Input:')}    ${colors.value(input.toLocaleString())}`,
    `  ${colors.key('Output:')}   ${colors.value(output.toLocaleString())}`,
    `  ${colors.key('Total:')}    ${colors.value(total.toLocaleString())}`,
    sep,
    '',
  ]
  return lines.join('\n')
}

// ── Runtime Status Header (Gap 1) ────────────────────────────

/**
 * Renders a persistent status header bar at the top of the REPL terminal.
 * Called after startup and after each /model switch to confirm active backend.
 *
 * @param {object} status - Payload from RuntimeHealthMonitor.getRuntimeStatus()
 * @returns {string} Formatted ANSI string ready for console.log / io.println
 */
export function renderStatusHeader(status) {
  const isOnline = status.endpointStatus === 'ONLINE'
  const statusColor = isOnline ? chalk.green : chalk.red
  const sidecarColor = status.sidecarStatus.startsWith('ACTIVE') ? chalk.green : chalk.yellow
  const termWidth = process.stdout.columns || 80
  const divider = colors.dim('─'.repeat(termWidth))

  const headerLine = [
    chalk.bold.inverse(' NEHANDA REPL '),
    chalk.dim('│'),
    `Model: ${chalk.bold.white(status.model)}`,
    `[${statusColor(status.endpointStatus)}${status.latency >= 0 ? chalk.dim(` ${status.latency}ms`) : ''}]`,
  ].join(' ')

  const subLine = [
    `Sidecar (laya_worker): ${sidecarColor(status.sidecarStatus)}`,
    chalk.dim('│'),
    `Context: ${chalk.white(status.contextUsage)}`,
    chalk.dim('│'),
    `Target: ${chalk.dim(status.endpoint)}`,
  ].join(' ')

  return [divider, headerLine, subLine, divider].join('\n')
}

/**
 * Visual feedback printed immediately after a /model switch.
 * Includes an endpoint probe result so silent backend failures surface at once.
 *
 * @param {string} oldModel
 * @param {string} newModel
 * @param {object} status - Payload from RuntimeHealthMonitor.getRuntimeStatus()
 * @returns {string}
 */
export function renderModelSwitchNotice(oldModel, newModel, status) {
  const badge = status.endpointStatus === 'ONLINE'
    ? chalk.green('[SUCCESS]')
    : chalk.red('[WARNING: ENDPOINT UNREACHABLE]')

  return [
    '',
    `${badge} Switched active reasoning model: ${chalk.bold(oldModel)} ➔ ${chalk.bold(newModel)}`,
    `  Endpoint Probe: ${chalk.dim(status.endpoint)} (${status.endpointStatus})`,
  ].join('\n')
}
