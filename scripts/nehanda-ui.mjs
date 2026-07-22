#!/usr/bin/env node
// nehanda-ui — Ink terminal UI for nehanda-cli
// Wraps nehanda-server chat completions API with a proper terminal interface.
// Model registry, bearer token, and auth config are all accessible via slash commands.

import fs from 'node:fs'
import os from 'node:os'
import path from 'node:path'
import { createRequire } from 'node:module'
import { fileURLToPath } from 'node:url'

const __dirname = path.dirname(fileURLToPath(import.meta.url))
const REPO_ROOT = path.resolve(__dirname, '..')

// Resolve deps from nehanda-cli's own node_modules
const require = createRequire(import.meta.url)
const React = (await import(`${REPO_ROOT}/node_modules/react/index.js`)).default
const { render, Box, Text, useApp, useInput } = await import(`${REPO_ROOT}/node_modules/ink/build/index.js`)
const { default: TextInput } = await import(`${REPO_ROOT}/node_modules/ink-text-input/build/index.js`)
const { default: chalk } = await import(`${REPO_ROOT}/node_modules/chalk/source/index.js`)
const { marked } = await import(`${REPO_ROOT}/node_modules/marked/lib/marked.esm.js`)
const { default: TerminalRenderer } = await import(`${REPO_ROOT}/node_modules/marked-terminal/index.js`)
const Database = require(`${REPO_ROOT}/node_modules/better-sqlite3`)

const e = React.createElement

// ── Config paths ─────────────────────────────────────────────
const AIMEE_DIR = path.join(os.homedir(), '.config', 'aimee')
const AIMEE_YAML = path.join(AIMEE_DIR, 'aimee.yaml')
const AGENTS_JSON = path.join(AIMEE_DIR, 'agents.json')
const AIMEE_DB = path.join(AIMEE_DIR, 'aimee.db')
const AICHAT_CFG = process.platform === 'darwin'
  ? path.join(os.homedir(), 'Library', 'Application Support', 'aichat', 'config.yaml')
  : path.join(os.homedir(), '.config', 'aichat', 'config.yaml')

// ── Config helpers ───────────────────────────────────────────
function readAgents() {
  try { return JSON.parse(fs.readFileSync(AGENTS_JSON, 'utf8')) }
  catch { return { agents: [], default_agent: '' } }
}

function readBearerToken() {
  try {
    const yaml = fs.readFileSync(AIMEE_YAML, 'utf8')
    const m = yaml.match(/bearer_token:\s*["']?([a-f0-9]+)["']?/)
    return m ? m[1] : null
  } catch { return null }
}

function writeBearerToken(token) {
  // Update aimee.yaml
  let yaml = fs.readFileSync(AIMEE_YAML, 'utf8')
  yaml = yaml.replace(/bearer_token:\s*["']?[a-f0-9]+["']?/, `bearer_token: ${token}`)
  fs.writeFileSync(AIMEE_YAML, yaml, 'utf8')

  // Update aichat config
  let aichat = fs.readFileSync(AICHAT_CFG, 'utf8')
  aichat = aichat.replace(/api_key:\s*["'][^"']*["']/, `api_key: "${token}"`)
  fs.writeFileSync(AICHAT_CFG, aichat, 'utf8')
}

function writeAgentModel(model) {
  const data = readAgents()
  const agent = data.agents?.find(a => a.name === data.default_agent) || data.agents?.[0]
  if (agent) agent.model = model
  fs.writeFileSync(AGENTS_JSON, JSON.stringify(data, null, 2), 'utf8')
}

function writeOrchestratorModel(model) {
  let yaml = fs.readFileSync(AIMEE_YAML, 'utf8')
  // Update the concurrency block key and the openai_model fallback
  yaml = yaml.replace(/openai_model:\s*\S+/, `openai_model: ${model}`)
  // Update concurrency per_model key if present
  const agent = readAgents().agents?.[0]
  if (agent?.model) {
    yaml = yaml.replace(new RegExp(`(per_model:\\s*\\n\\s*)${agent.model.replace(/[.*+?^${}()|[\]\\]/g, '\\$&')}:`), `$1${model}:`)
  }
  fs.writeFileSync(AIMEE_YAML, yaml, 'utf8')
}

function readTokenStats(since) {
  try {
    const db = new Database(AIMEE_DB, { readonly: true, fileMustExist: true })
    const rows = db.prepare(`
      SELECT SUM(prompt_tokens) as input, SUM(completion_tokens) as output, COUNT(*) as calls
      FROM token_audit WHERE created_at >= ?
    `).get(since)
    db.close()
    return { input: rows?.input || 0, output: rows?.output || 0, calls: rows?.calls || 0 }
  } catch { return { input: 0, output: 0, calls: 0 } }
}

// ── Markdown ─────────────────────────────────────────────────
marked.setOptions({
  renderer: new TerminalRenderer({
    code: chalk.yellow,
    codespan: chalk.yellow,
    strong: chalk.bold,
    em: chalk.italic,
    heading: chalk.bold.cyan,
    hr: () => chalk.dim('─'.repeat(50)),
    listitem: text => `  ${chalk.dim('•')} ${text}`,
    paragraph: text => text + '\n',
    link: (href, _title, text) => `${text} ${chalk.dim.underline(href)}`,
  })
})

function renderMd(text) {
  if (!text) return ''
  try { return marked(text).replace(/\n{3,}/g, '\n\n').trimEnd() }
  catch { return text }
}

// ── Constants ────────────────────────────────────────────────
const SPINNER_FRAMES = ['⠋', '⠙', '⠹', '⠸', '⠼', '⠴', '⠦', '⠧', '⠇', '⠏']

const SLASH_COMMANDS = [
  { name: '/model',   desc: 'Change model (aimee agent or orchestrator)' },
  { name: '/bearer',  desc: 'Change bearer token' },
  { name: '/token',   desc: 'Session token usage' },
  { name: '/auth',    desc: 'Auth status / login stub' },
  { name: '/config',  desc: 'Show current config' },
  { name: '/clear',   desc: 'New conversation' },
  { name: '/help',    desc: 'Show commands' },
  { name: '/exit',    desc: 'Quit' },
]

// ── Cape Town skyline (from ona-code) ─────────────────────────
function CapeTownSkyline() {
  return e(Box, { flexDirection: 'column', alignItems: 'center' },
    e(Text, {},
      e(Text, { dimColor: true }, '       ✦  '),
      e(Text, { color: 'yellow' }, '☀'),
    ),
    e(Text, {},
      e(Text, { color: '#cc785c' }, '  ▄▄▄▄▄▄▄▄▄▄'),
      e(Text, { dimColor: true }, '  ✦ '),
      e(Text, { color: '#cc785c' }, '▲'),
    ),
    e(Text, {},
      e(Text, { color: '#cc785c' }, '  ██████████▌   ▟▊'),
    ),
    e(Text, {},
      e(Text, { color: '#cc785c' }, ' ▟████████████▄█▊▖'),
    ),
    e(Text, {},
      e(Text, { color: '#8a6040' }, ' ░▒█▒░▓█▓░▒█▒░▓█▓░'),
    ),
  )
}

// ── Welcome Banner ───────────────────────────────────────────
function WelcomeBanner({ agent, bearerToken }) {
  let cols = 80
  try { cols = process.stdout.columns || 80 } catch {}
  const bannerWidth = Math.min(cols - 2, 80)
  const leftWidth = Math.floor(bannerWidth * 0.42)
  const rightWidth = bannerWidth - leftWidth - 3

  const modelLine = agent?.model || '(no agent)'
  const endpoint = agent?.endpoint || '(no endpoint)'

  return e(Box, {
    flexDirection: 'column', borderStyle: 'single', borderColor: '#cc785c',
    width: bannerWidth, marginBottom: 1,
  },
    e(Box, { flexDirection: 'row', paddingX: 1 },
      // Left panel
      e(Box, { flexDirection: 'column', width: leftWidth, alignItems: 'center', paddingY: 1 },
        e(Text, { bold: true, color: '#cc785c' }, 'Nehanda'),
        e(CapeTownSkyline, {}),
        e(Text, { dimColor: true }, modelLine),
      ),
      // Separator
      e(Box, {
        width: 1, borderStyle: 'single', borderColor: '#cc785c',
        borderTop: false, borderBottom: false, borderRight: false, borderLeft: true,
      }),
      // Right panel
      e(Box, { flexDirection: 'column', width: rightWidth, paddingLeft: 1, paddingY: 1 },
        e(Text, { bold: true, color: '#cc785c' }, 'Configuration'),
        e(Box, { marginTop: 1, flexDirection: 'column' },
          e(Text, {},
            e(Text, { dimColor: true }, 'Agent:    '),
            e(Text, { color: 'cyan' }, agent?.name || '—'),
          ),
          e(Text, {},
            e(Text, { dimColor: true }, 'Model:    '),
            e(Text, { bold: true }, modelLine),
          ),
          e(Text, {},
            e(Text, { dimColor: true }, 'Endpoint: '),
            e(Text, { dimColor: true }, endpoint),
          ),
          e(Text, {},
            e(Text, { dimColor: true }, 'Bearer:   '),
            e(Text, { dimColor: true }, bearerToken ? bearerToken.slice(0, 8) + '…' : '(none)'),
          ),
        ),
        e(Box, { marginTop: 1 },
          e(Text, { dimColor: true }, 'Type /help for commands'),
        ),
      ),
    ),
  )
}

// ── Message view ─────────────────────────────────────────────
function MessageView({ msg }) {
  if (msg.role === 'user') {
    return e(Box, { marginLeft: 0, flexDirection: 'row' },
      e(Text, { bold: true, color: '#cc785c' }, '❯ '),
      e(Box, { flexShrink: 1 }, e(Text, { wrap: 'wrap' }, msg.text)),
    )
  }
  if (msg.role === 'assistant') {
    return e(Box, { flexDirection: 'column', marginLeft: 0 },
      e(Text, {}, renderMd(msg.text)),
    )
  }
  if (msg.role === 'system') {
    return e(Box, { marginLeft: 1, marginTop: 0 },
      e(Text, { dimColor: true }, msg.text),
    )
  }
  if (msg.role === 'error') {
    return e(Box, { marginLeft: 1 },
      e(Text, { color: 'red' }, '✗ ' + msg.text),
    )
  }
  return null
}

// ── Spinner ──────────────────────────────────────────────────
function Spinner({ label }) {
  const [frame, setFrame] = React.useState(0)
  React.useEffect(() => {
    const t = setInterval(() => setFrame(f => (f + 1) % SPINNER_FRAMES.length), 80)
    return () => clearInterval(t)
  }, [])
  return e(Box, { marginLeft: 1 },
    e(Text, { color: '#cc785c' }, SPINNER_FRAMES[frame] + ' '),
    e(Text, { dimColor: true }, label || 'Thinking…'),
  )
}

// ── Slash menu typeahead ──────────────────────────────────────
function SlashMenu({ filter }) {
  const matches = SLASH_COMMANDS.filter(c =>
    c.name.startsWith(filter) || c.name.includes(filter.slice(1))
  )
  if (!matches.length) return null
  return e(Box, { flexDirection: 'column', marginLeft: 2 },
    ...matches.map((c, i) =>
      e(Box, { key: String(i) },
        e(Text, { color: 'cyan' }, c.name.padEnd(12)),
        e(Text, { dimColor: true }, c.desc),
      )
    )
  )
}

// ── Token stats footer ────────────────────────────────────────
function TokenFooter({ stats, sessionStart }) {
  const { input = 0, output = 0, calls = 0 } = stats
  let cols = 80
  try { cols = process.stdout.columns || 80 } catch {}

  return e(Box, { flexDirection: 'column' },
    e(Text, { dimColor: true }, '─'.repeat(cols)),
    e(Box, { flexDirection: 'row', paddingX: 1, justifyContent: 'space-between' },
      e(Text, { dimColor: true }, '↑ ' + input.toLocaleString() + '  ↓ ' + output.toLocaleString() + '  calls ' + calls),
      e(Text, { dimColor: true }, '? /help  ^C exit'),
    ),
  )
}

// ── Input area ───────────────────────────────────────────────
function InputArea({ onSubmit, isLoading }) {
  const [value, setValue] = React.useState('')
  const showMenu = value.startsWith('/') && !value.includes(' ')

  const handleSubmit = React.useCallback((v) => {
    if (!v.trim()) return
    setValue('')
    onSubmit(v.trim())
  }, [onSubmit])

  if (isLoading) return null

  return e(Box, { flexDirection: 'column' },
    showMenu ? e(SlashMenu, { filter: value }) : null,
    e(Box, { paddingX: 1 },
      e(Text, { bold: true, color: '#cc785c' }, '❯ '),
      e(TextInput, { value, onChange: setValue, onSubmit: handleSubmit }),
    ),
  )
}

// ── Multi-step prompt (for /model, /bearer) ───────────────────
function AskPrompt({ question, onAnswer }) {
  const [value, setValue] = React.useState('')
  return e(Box, { flexDirection: 'column' },
    e(Box, { marginLeft: 1 }, e(Text, { dimColor: true }, question)),
    e(Box, { paddingX: 1 },
      e(Text, { color: '#cc785c' }, '❯ '),
      e(TextInput, {
        value,
        onChange: setValue,
        onSubmit: React.useCallback(v => onAnswer(v.trim()), [onAnswer]),
      }),
    ),
  )
}

// ── Main App ──────────────────────────────────────────────────
function App({ bridge }) {
  const { exit } = useApp()
  const [messages, setMessages] = React.useState([])
  const [isLoading, setIsLoading] = React.useState(false)
  const [askState, setAskState] = React.useState(null)
  const [stats, setStats] = React.useState({ input: 0, output: 0, calls: 0 })
  const [agentCfg, setAgentCfg] = React.useState(() => {
    const data = readAgents()
    return data.agents?.find(a => a.name === data.default_agent) || data.agents?.[0] || null
  })
  const [bearerToken, setBearerToken] = React.useState(readBearerToken)
  const sessionStart = React.useRef(new Date().toISOString().replace('T', ' ').replace(/\.\d+Z$/, ''))

  // Poll token stats every 5s
  React.useEffect(() => {
    const poll = () => setStats(readTokenStats(sessionStart.current))
    poll()
    const t = setInterval(poll, 5000)
    return () => clearInterval(t)
  }, [])

  // Wire bridge
  React.useEffect(() => {
    bridge.addMessage = msg => setMessages(prev => [...prev, msg])
    bridge.setLoading = v => setIsLoading(v)
    bridge.ask = (question) => new Promise(resolve => setAskState({ question, resolve }))
    bridge.exit = () => { exit() }
    bridge.refreshAgent = () => {
      const data = readAgents()
      setAgentCfg(data.agents?.find(a => a.name === data.default_agent) || data.agents?.[0] || null)
    }
    bridge.refreshBearer = () => setBearerToken(readBearerToken())
  }, [bridge, exit])

  const handleAnswer = React.useCallback((answer) => {
    if (askState?.resolve) {
      setMessages(prev => [...prev, { role: 'system', text: askState.question + ' ' + answer }])
      askState.resolve(answer)
    }
    setAskState(null)
  }, [askState])

  return e(Box, { flexDirection: 'column' },
    e(WelcomeBanner, { agent: agentCfg, bearerToken }),
    ...messages.map((msg, i) => e(MessageView, { key: String(i), msg })),
    isLoading ? e(Spinner, {}) : null,
    askState
      ? e(AskPrompt, { question: askState.question, onAnswer: handleAnswer })
      : e(InputArea, { onSubmit: bridge.onSubmit, isLoading }),
    e(TokenFooter, { stats, sessionStart: sessionStart.current }),
  )
}

// ── Chat completion ───────────────────────────────────────────
async function streamChat(messages, bearerToken, bridge) {
  const url = 'http://127.0.0.1:8740/v1/chat/completions'
  const body = { model: 'nehanda', messages, stream: true }

  const res = await fetch(url, {
    method: 'POST',
    headers: {
      'Content-Type': 'application/json',
      'Authorization': `Bearer ${bearerToken}`,
    },
    body: JSON.stringify(body),
  })

  if (!res.ok) {
    const t = await res.text()
    throw new Error(`${res.status}: ${t.slice(0, 500)}`)
  }

  const reader = res.body.getReader()
  const decoder = new TextDecoder()
  let buffer = '', fullText = ''

  while (true) {
    const { done, value } = await reader.read()
    if (done) break
    buffer += decoder.decode(value, { stream: true })
    const parts = buffer.split('\n')
    buffer = parts.pop() ?? ''
    for (const line of parts) {
      const trimmed = line.trim()
      if (!trimmed.startsWith('data:')) continue
      const data = trimmed.slice(5).trim()
      if (data === '[DONE]') continue
      let json
      try { json = JSON.parse(data) } catch { continue }
      const content = json.choices?.[0]?.delta?.content
      if (content) fullText += content
    }
  }

  // Strip thinking traces
  let text = fullText
  text = text.replace(/<think>[\s\S]*?<\/think>/g, '')
  text = text.replace(/<think>[\s\S]*/g, '')
  return text.trim()
}

// ── Command handler ───────────────────────────────────────────
async function handleCommand(cmd, bridge) {
  const parts = cmd.split(/\s+/)
  const name = parts[0]

  if (name === '/help') {
    const lines = SLASH_COMMANDS.map(c =>
      `  ${chalk.cyan(c.name.padEnd(12))} ${chalk.dim(c.desc)}`
    ).join('\n')
    bridge.addMessage({ role: 'system', text: '\nCommands:\n' + lines + '\n' })
    return
  }

  if (name === '/config') {
    const data = readAgents()
    const agent = data.agents?.find(a => a.name === data.default_agent) || data.agents?.[0]
    const bearer = readBearerToken()
    bridge.addMessage({ role: 'system', text: [
      '',
      `  Agent:    ${chalk.cyan(agent?.name || '—')}`,
      `  Model:    ${chalk.bold(agent?.model || '—')}`,
      `  Endpoint: ${chalk.dim(agent?.endpoint || '—')}`,
      `  Tools:    ${agent?.tools_enabled ? 'enabled' : 'disabled'}`,
      `  Bearer:   ${bearer ? chalk.dim(bearer.slice(0, 8) + '…') : chalk.red('(none)')}`,
      '',
    ].join('\n') })
    return
  }

  if (name === '/token') {
    const since = new Date(Date.now() - 86400 * 1000).toISOString().replace('T', ' ').replace(/\.\d+Z$/, '')
    const stats = readTokenStats(since)
    bridge.addMessage({ role: 'system', text: [
      '',
      `  ${chalk.bold('Last 24h token usage')}`,
      `  Input:  ${chalk.white(stats.input.toLocaleString())}`,
      `  Output: ${chalk.white(stats.output.toLocaleString())}`,
      `  Total:  ${chalk.white((stats.input + stats.output).toLocaleString())}`,
      `  Calls:  ${chalk.white(String(stats.calls))}`,
      '',
    ].join('\n') })
    return
  }

  if (name === '/auth') {
    bridge.addMessage({ role: 'system', text: [
      '',
      `  ${chalk.bold('Auth')}`,
      `  Auth infrastructure not yet configured.`,
      `  Run ${chalk.cyan('nehanda auth login')} when available.`,
      `  Current bearer token: ${readBearerToken() ? chalk.green('set') : chalk.red('not set')}`,
      '',
    ].join('\n') })
    return
  }

  if (name === '/clear') {
    bridge.addMessage({ role: 'system', text: '  Conversation cleared.' })
    bridge.history = []
    return
  }

  if (name === '/exit' || name === '/quit') {
    bridge.exit()
    process.exit(0)
  }

  if (name === '/bearer') {
    const answer = await bridge.ask('New bearer token (hex string):')
    if (!answer) return
    try {
      writeBearerToken(answer)
      bridge.refreshBearer()
      bridge.addMessage({ role: 'system', text: `  ${chalk.green('✓')} Bearer token updated in aimee.yaml and aichat config.` })
    } catch (err) {
      bridge.addMessage({ role: 'error', text: 'Failed to write token: ' + err.message })
    }
    return
  }

  if (name === '/model') {
    const choice = await bridge.ask('Change: [1] aimee agent model  [2] orchestrator model  (1/2):')
    if (choice === '1') {
      const current = readAgents().agents?.[0]?.model || ''
      const model = await bridge.ask(`New agent model (current: ${current}):`)
      if (!model) return
      try {
        writeAgentModel(model)
        bridge.refreshAgent()
        bridge.addMessage({ role: 'system', text: `  ${chalk.green('✓')} Agent model updated to ${chalk.bold(model)}` })
      } catch (err) {
        bridge.addMessage({ role: 'error', text: 'Failed: ' + err.message })
      }
    } else if (choice === '2') {
      const yaml = fs.readFileSync(AIMEE_YAML, 'utf8')
      const m = yaml.match(/openai_model:\s*(\S+)/)
      const current = m ? m[1] : ''
      const model = await bridge.ask(`New orchestrator model (current: ${current}):`)
      if (!model) return
      try {
        writeOrchestratorModel(model)
        bridge.addMessage({ role: 'system', text: `  ${chalk.green('✓')} Orchestrator model updated to ${chalk.bold(model)}` })
      } catch (err) {
        bridge.addMessage({ role: 'error', text: 'Failed: ' + err.message })
      }
    }
    return
  }

  bridge.addMessage({ role: 'system', text: `  Unknown command: ${name}. Type /help for commands.` })
}

// ── Entry point ───────────────────────────────────────────────
const bridge = { history: [] }

bridge.onSubmit = async (text) => {
  bridge.addMessage({ role: 'user', text })

  if (text.startsWith('/')) {
    await handleCommand(text, bridge)
    return
  }

  bridge.setLoading(true)
  bridge.history.push({ role: 'user', content: text })

  try {
    const bearer = readBearerToken()
    if (!bearer) throw new Error('No bearer token configured. Run /bearer to set one.')
    const reply = await streamChat(bridge.history, bearer, bridge)
    bridge.history.push({ role: 'assistant', content: reply })
    bridge.addMessage({ role: 'assistant', text: reply })
  } catch (err) {
    bridge.addMessage({ role: 'error', text: err.message })
    bridge.history.pop() // remove failed user message from history
  } finally {
    bridge.setLoading(false)
  }
}

render(e(App, { bridge }), { exitOnCtrlC: true })
