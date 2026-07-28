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
// REPO_ROOT: if installed to ~/.local/bin, node_modules lives in nehanda-cli repo.
// Resolve by checking for node_modules relative to __dirname, then walk up.
function findRepoRoot(start) {
  let dir = start
  for (let i = 0; i < 6; i++) {
    if (fs.existsSync(path.join(dir, 'node_modules', 'ink'))) return dir
    const parent = path.dirname(dir)
    if (parent === dir) break
    dir = parent
  }
  // Fallback: check the scripts dir sibling (dev layout)
  const sibling = path.resolve(start, '..', '..', 'nehanda-cli')
  if (fs.existsSync(path.join(sibling, 'node_modules', 'ink'))) return sibling
  throw new Error(`Cannot find nehanda-cli node_modules. Re-run install.sh.`)
}
const REPO_ROOT = findRepoRoot(__dirname)

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

function writeOrchestratorModel(model) {
  let yaml = fs.readFileSync(AIMEE_YAML, 'utf8')
  yaml = yaml.replace(/openai_model:\s*\S+/, `openai_model: ${model}`)
  const agent = readAgents().agents?.[0]
  if (agent?.model) {
    yaml = yaml.replace(new RegExp(`(per_model:\\s*\\n\\s*)${agent.model.replace(/[.*+?^${}()|[\]\\]/g, '\\$&')}:`), `$1${model}:`)
  }
  fs.writeFileSync(AIMEE_YAML, yaml, 'utf8')
}

function recordTokenAudit(promptTokens, completionTokens, modelName) {
  try {
    const db = new Database(AIMEE_DB)
    const now = new Date().toISOString().replace("T", " ").replace(/\.\d+Z$/, "")
    const agent = readAgents().agents?.[0]
    db.prepare(`
      INSERT INTO token_audit (
        session_id, user_id, org_id, agent_name, agent_provider,
        agent_model, flow_stage, prompt_tokens, completion_tokens,
        total_tokens, created_at
      ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    `).run(
      "ui-session", "user", "org", agent?.name || "nehanda", agent?.provider || "openai",
      modelName || agent?.model || "GLM-4.7-Flash", "stream_chat",
      promptTokens, completionTokens, promptTokens + completionTokens, now
    )
    db.close()
  } catch {}
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
  { name: '/stats',   desc: 'KB, memory, token, and health statistics' },
  { name: '/auth',    desc: 'Auth status / login stub' },
  { name: '/config',  desc: 'Show current config' },
  { name: '/clear',   desc: 'New conversation' },
  { name: '/help',    desc: 'Show commands' },
  { name: '/exit',    desc: 'Quit' },
]

// ── Model registry ────────────────────────────────────────────
const REGISTRY_PATH = path.join(AIMEE_DIR, 'model-registry.json')

const DEFAULT_REGISTRY = {
  ollama_hosts: ['http://AsobaCorp-1.local:11434'],
  models: [
    { name: 'nehanda-rag-synthesis-27b', endpoint: 'http://nehanda.asoba.co:8000', label: 'Nehanda 27B (EC2)', desc: 'Primary — fine-tuned Qwen3.6 27B, vLLM, af-south-1' },
    { name: 'deepseek-coder-v2:latest', endpoint: 'http://AsobaCorp-1.local:11434/v1', label: 'DeepSeek Coder V2 (LAN)', desc: 'Windows LAN delegate — coding specialist' },
    { name: 'qwen2.5:14b', endpoint: 'http://AsobaCorp-1.local:11434/v1', label: 'Qwen 2.5 14B (LAN)', desc: 'Windows LAN delegate — general reasoning' },
    { name: 'deepseek-r1:14b', endpoint: 'http://AsobaCorp-1.local:11434/v1', label: 'DeepSeek R1 14B (LAN)', desc: 'Windows LAN delegate — deep reasoning' },
  ],
}

function readRegistry() {
  try {
    return JSON.parse(fs.readFileSync(REGISTRY_PATH, 'utf8'))
  } catch {
    return DEFAULT_REGISTRY
  }
}

async function discoverOllamaModels(hosts) {
  const discovered = []
  await Promise.all(hosts.map(async (host) => {
    const base = host.replace(/\/$/, '')
    try {
      const res = await fetch(`${base}/api/tags`, {
        signal: AbortSignal.timeout(2000),
      })
      if (!res.ok) return
      const data = await res.json()
      for (const m of (data.models || [])) {
        discovered.push({
          name: m.name,
          endpoint: `${base}/v1`,
          label: `${m.name} (${new URL(base).hostname})`,
          desc: `Discovered via Ollama — ${m.name}`,
          discovered: true,
        })
      }
    } catch { /* host unreachable — skip silently */ }
  }))
  return discovered
}

async function buildModelList() {
  const registry = readRegistry()
  const base = registry.models || []
  const hosts = registry.ollama_hosts || []

  const discovered = await discoverOllamaModels(hosts)
  const baseKeys = new Set(base.map(m => `${m.name}|${m.endpoint}`))
  const newModels = discovered.filter(m => !baseKeys.has(`${m.name}|${m.endpoint}`))

  return [...base, ...newModels]
}

// ── Cape Town skyline (from ona-code) ─────────────────────────
function CapeTownSkyline() {
  return e(Box, { flexDirection: 'column', alignItems: 'center' },
    e(Text, {},
      e(Text, { dimColor: true }, '        ✦  '),
      e(Text, { color: 'yellow' }, '☀'),
    ),
    e(Text, {},
      e(Text, { color: '#cc785c' }, '  ▄▄▄▄▄▄▄▄▄▄'),
      e(Text, { dimColor: true }, '  ✦ '),
      e(Text, { color: '#cc785c' }, '▲'),
    ),
    e(Text, {},
      e(Text, { color: '#cc785c' }, '  ██████████▌    ▟▊'),
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
      e(Box, { flexDirection: 'column', width: leftWidth, alignItems: 'center', paddingY: 1 },
        e(Text, { bold: true, color: '#cc785c' }, 'Nehanda'),
        e(CapeTownSkyline, {}),
        e(Text, { dimColor: true }, modelLine),
      ),
      e(Box, {
        width: 1, borderStyle: 'single', borderColor: '#cc785c',
        borderTop: false, borderBottom: false, borderRight: false, borderLeft: true,
      }),
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

// ── Stats queries ─────────────────────────────────────────────
import { execFile } from 'node:child_process'
import { promisify } from 'node:util'
const execFileAsync = promisify(execFile)

async function psqlQuery(sql) {
  try {
    const { stdout } = await execFileAsync('psql', ['-d', 'aimee_shared', '-t', '-A', '-F', '\t', '-c', sql], { timeout: 5000 })
    return stdout.trim()
  } catch { return null }
}

async function sqliteQuery(sql) {
  try {
    const { stdout } = await execFileAsync('sqlite3', [AIMEE_DB, '-separator', '\t', sql], { timeout: 5000 })
    return stdout.trim()
  } catch { return null }
}

async function fetchStats() {
  const [
    kbRaw, memRaw, tokenTodayRaw, token7dRaw, tokenAllRaw,
    kbHealth, embedderHealth
  ] = await Promise.all([
    psqlQuery(`
      SELECT
        (SELECT COUNT(*) FROM kb_documents) AS docs,
        (SELECT COUNT(*) FROM kb_embeddings) AS embeddings,
        (SELECT COUNT(*) FROM files) AS files,
        (SELECT COUNT(*) FROM entity_edges) AS entity_edges,
        (SELECT COUNT(*) FROM code_embeddings) AS code_embeddings,
        (SELECT COUNT(*) FROM artifacts) AS artifacts
    `),
    psqlQuery(`
      SELECT
        (SELECT COUNT(*) FROM memory_units) AS units,
        (SELECT COUNT(*) FROM memory_embeddings) AS embeddings,
        (SELECT COUNT(*) FROM memory_episodes) AS episodes,
        (SELECT COUNT(*) FROM memory_summaries) AS summaries,
        (SELECT COUNT(*) FROM memory_relations) AS relations,
        (SELECT COUNT(*) FROM memories) AS raw
    `),
    sqliteQuery(`SELECT COALESCE(SUM(prompt_tokens),0), COALESCE(SUM(completion_tokens),0), COUNT(*) FROM token_audit WHERE created_at >= date('now')`),
    sqliteQuery(`SELECT COALESCE(SUM(prompt_tokens),0), COALESCE(SUM(completion_tokens),0), COUNT(*) FROM token_audit WHERE created_at >= date('now','-7 days')`),
    sqliteQuery(`SELECT COALESCE(SUM(prompt_tokens),0), COALESCE(SUM(completion_tokens),0), COUNT(*) FROM token_audit`),
    fetch('http://127.0.0.1:8741/v1/health?status=1', { signal: AbortSignal.timeout(2000) })
      .then(r => r.json()).catch(() => null),
    fetch('http://127.0.0.1:8742/health', { signal: AbortSignal.timeout(2000) })
      .then(r => r.json()).catch(() => null),
  ])

  const kb = kbRaw ? kbRaw.split('\t') : Array(6).fill('0')
  const mem = memRaw ? memRaw.split('\t') : Array(6).fill('0')

  const [todayIn=0, todayOut=0, todayCalls=0] = (tokenTodayRaw||'').split('\t').map(Number)
  const [d7In=0, d7Out=0, d7Calls=0] = (token7dRaw||'').split('\t').map(Number)
  const [allIn=0, allOut=0, allCalls=0] = (tokenAllRaw||'').split('\t').map(Number)

  return {
    kb: {
      docs: parseInt(kb[0])||0,
      embeddings: parseInt(kb[1])||0,
      files: parseInt(kb[2])||0,
      entityEdges: parseInt(kb[3])||0,
      codeEmbeddings: parseInt(kb[4])||0,
      artifacts: parseInt(kb[5])||0,
      available: kbHealth?.available === true,
    },
    memory: {
      units: parseInt(mem[0])||0,
      embeddings: parseInt(mem[1])||0,
      episodes: parseInt(mem[2])||0,
      summaries: parseInt(mem[3])||0,
      relations: parseInt(mem[4])||0,
      raw: parseInt(mem[5])||0,
    },
    tokens: {
      today: { in: todayIn, out: todayOut, calls: todayCalls },
      week:  { in: d7In,    out: d7Out,    calls: d7Calls },
      all:   { in: allIn,   out: allOut,   calls: allCalls },
    },
    health: {
      kb: kbHealth != null,
      embedder: embedderHealth != null,
    },
    _kbHealth: kbHealth,
    _embedderModel: embedderHealth?.model || null,
  }
}

function TokenFooter({ stats }) {
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

// ── Multi-step prompt (for /bearer) ──────────────────────────
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

// ── Select menu (for /model) ─────────────────────────────────
function SelectMenu({ title, items, onSelect, onCancel }) {
  useInput((input, key) => {
    if (key.escape) { onCancel(); return }
    const n = parseInt(input, 10)
    if (!isNaN(n) && n >= 1 && n <= items.length) onSelect(items[n - 1])
  })

  return e(Box, { flexDirection: 'column', marginLeft: 1 },
    e(Text, { bold: true, color: '#cc785c' }, title),
    e(Box, { marginTop: 1, flexDirection: 'column' },
      ...items.map((item, i) =>
        e(Box, { key: String(i), flexDirection: 'column', marginBottom: 0 },
          e(Box, {},
            e(Text, { color: 'cyan' }, `  ${i + 1}. `),
            e(Text, { bold: true }, item.label),
          ),
          e(Box, { marginLeft: 5 },
            e(Text, { dimColor: true }, item.desc),
          ),
        )
      ),
    ),
    e(Box, { marginTop: 1 },
      e(Text, { dimColor: true }, 'Press 1–' + items.length + ' to select, Esc to cancel'),
    ),
  )
}

// ── Main App ──────────────────────────────────────────────────
function App({ bridge }) {
  const { exit } = useApp()
  const [messages, setMessages] = React.useState([])
  const [isLoading, setIsLoading] = React.useState(false)
  const [askState, setAskState] = React.useState(null)
  const [selectState, setSelectState] = React.useState(null)
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
    bridge.setMessages = setMessages
    bridge.setLoading = v => setIsLoading(v)
    bridge.ask = (question) => new Promise(resolve => setAskState({ question, resolve }))
    bridge.select = (title, items) => new Promise(resolve => setSelectState({ title, items, resolve }))
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

  const handleSelect = React.useCallback((item) => {
    if (selectState?.resolve) {
      setMessages(prev => [...prev, { role: 'system', text: `  Selected: ${item.label}` }])
      selectState.resolve(item)
    }
    setSelectState(null)
  }, [selectState])

  const handleSelectCancel = React.useCallback(() => {
    if (selectState?.resolve) selectState.resolve(null)
    setSelectState(null)
  }, [selectState])

  return e(Box, { flexDirection: 'column' },
    e(WelcomeBanner, { agent: agentCfg, bearerToken }),
    ...messages.map((msg, i) => e(MessageView, { key: String(i), msg })),
    isLoading ? e(Spinner, {}) : null,
    askState
      ? e(AskPrompt, { question: askState.question, onAnswer: handleAnswer })
      : selectState
        ? e(SelectMenu, { title: selectState.title, items: selectState.items, onSelect: handleSelect, onCancel: handleSelectCancel })
        : e(InputArea, { onSubmit: bridge.onSubmit, isLoading }),
    e(TokenFooter, { stats }),
  )
}

// ── Diff Parser & File Executor ──────────────────────────────
function applyDiffBlocks(text, bridge) {
  const results = { applied: 0, failed: 0, errors: [] }
  const diffRegex = /^(.+?\.(?:\w+))\s*\n<<<<<<< SEARCH\n([\s\S]*?)\n?=======\n([\s\S]*?)\n?>>>>>>> REPLACE/gm

  let match
  while ((match = diffRegex.exec(text)) !== null) {
    const [, filepath, searchContent, replaceContent] = match

    try {
      const targetPath = path.isAbsolute(filepath)
        ? filepath
        : path.resolve(process.cwd(), filepath)

      if (!fs.existsSync(targetPath)) {
        results.failed++
        results.errors.push(`✗ File not found: ${filepath}`)
        continue
      }

      const fileContent = fs.readFileSync(targetPath, 'utf-8')

      if (!fileContent.includes(searchContent)) {
        results.failed++
        results.errors.push(`✗ Search block not found in: ${filepath}`)
        continue
      }

      const newContent = fileContent.replace(searchContent, replaceContent)
      fs.writeFileSync(targetPath, newContent, 'utf-8')

      results.applied++
      bridge.addMessage({ role: 'system', text: `  ${chalk.green('✓')} Applied diff to ${chalk.bold(filepath)}` })

    } catch (err) {
      results.failed++
      results.errors.push(`  ${chalk.red('✗')} Error processing ${filepath}: ${err.message}`)
    }
  }

  if (results.errors.length > 0) {
    results.errors.forEach(e => bridge.addMessage({ role: 'error', text: e }))
  }

  return results
}

// ── Chat completion ───────────────────────────────────────────
async function streamChat(messages, bearerToken, onChunk) {
  const url = 'http://127.0.0.1:8740/v1/chat/completions'
  const body = { model: 'nehanda', messages, stream: true, cwd: process.cwd() }

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
      if (content) {
        fullText += content
        let cleanText = fullText
          .replace(/<think>[\s\S]*?<\/think>/g, '')
          .replace(/<think>[\s\S]*/g, '')
        if (onChunk) onChunk(cleanText.trimStart())
      }
    }
  }

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

  if (name === '/stats') {
    bridge.addMessage({ role: 'system', text: '  Gathering stats…' })
    let s
    try {
      s = await fetchStats()
    } catch (err) {
      bridge.addMessage({ role: 'error', text: 'Stats failed: ' + err.message })
      return
    }

    const ok  = chalk.green('●')
    const off = chalk.red('○')
    const num = n => chalk.white(Number(n).toLocaleString())
    const dim = chalk.dim

    const kbHealth = s._kbHealth
    const ingest = kbHealth?.ingest_queue || {}
    const vec = kbHealth?.vector || {}

    const lines = [
      '',
      chalk.bold.underline('Knowledge Base'),
      `  ${s.health.kb ? ok : off} Service          ${s.health.kb ? chalk.green('available') : chalk.red('unavailable')}`,
      `  ${dim('Documents:')}       ${num(s.kb.docs)}`,
      `  ${dim('Embeddings:')}      ${num(s.kb.embeddings)}`,
      `  ${dim('Files indexed:')}   ${num(s.kb.files)}`,
      `  ${dim('Entity edges:')}    ${num(s.kb.entityEdges)}`,
      `  ${dim('Code embeddings:')} ${num(s.kb.codeEmbeddings)}`,
      `  ${dim('Artifacts:')}       ${num(s.kb.artifacts)}`,
      ...(Object.keys(ingest).length ? [
        `  ${dim('Ingest queue:')}    pending ${num(ingest.pending||0)}  done 24h ${num(ingest.done_last_24h||0)}  failed ${num(ingest.failed_last_24h||0)}`,
      ] : []),
      '',
      chalk.bold.underline('Memory'),
      `  ${dim('Units:')}           ${num(s.memory.units)}`,
      `  ${dim('Embeddings:')}      ${num(s.memory.embeddings)}`,
      `  ${dim('Episodes:')}        ${num(s.memory.episodes)}`,
      `  ${dim('Summaries:')}       ${num(s.memory.summaries)}`,
      `  ${dim('Relations:')}       ${num(s.memory.relations)}`,
      '',
      chalk.bold.underline('Token Usage'),
      `  ${''.padEnd(12)}  ${chalk.dim('in'.padStart(10))}  ${chalk.dim('out'.padStart(10))}  ${chalk.dim('calls')}`,
      `  ${'Today'.padEnd(12)}  ${String(s.tokens.today.in).padStart(10)}  ${String(s.tokens.today.out).padStart(10)}  ${s.tokens.today.calls}`,
      `  ${'Last 7 days'.padEnd(12)}  ${String(s.tokens.week.in).padStart(10)}  ${String(s.tokens.week.out).padStart(10)}  ${s.tokens.week.calls}`,
      `  ${'All time'.padEnd(12)}  ${String(s.tokens.all.in).padStart(10)}  ${String(s.tokens.all.out).padStart(10)}  ${s.tokens.all.calls}`,
      '',
      chalk.bold.underline('Services'),
      `  ${s.health.kb        ? ok : off} KB         :8741  ${vec.backend ? chalk.dim('(' + vec.backend + ')') : ''}`,
      `  ${s.health.embedder ? ok : off} Embedder   :8742  ${s._embedderModel ? chalk.dim('(' + s._embedderModel + ')') : ''}`,
      '',
    ]
    bridge.addMessage({ role: 'system', text: lines.join('\n') })
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
    const choice = await bridge.select('Select target', [
      { label: 'Aimee agent model', desc: 'Updates agents.json — the model nehanda-server uses for inference', value: 'agent' },
      { label: 'Orchestrator model', desc: 'Updates aimee.yaml openai_model — used for planning/delegation', value: 'orchestrator' },
    ])
    if (!choice) return

    const currentModel = choice.value === 'agent'
      ? (readAgents().agents?.[0]?.model || '')
      : (() => { const m = fs.readFileSync(AIMEE_YAML, 'utf8').match(/openai_model:\s*(\S+)/); return m ? m[1] : '' })()

    bridge.addMessage({ role: 'system', text: '  Discovering models…' })
    const allModels = await buildModelList()

    const registryItems = allModels.map(m => ({
      ...m,
      label: m.name === currentModel ? `${m.label} ✓` : m.label,
    }))

    const selected = await bridge.select(
      `Select model for ${choice.label}`,
      registryItems,
    )
    if (!selected) return

    try {
      if (choice.value === 'agent') {
        const data = readAgents()
        const agent = data.agents?.find(a => a.name === data.default_agent) || data.agents?.[0]
        if (agent) {
          agent.endpoint = selected.endpoint
          agent.model = selected.name
        }
        fs.writeFileSync(AGENTS_JSON, JSON.stringify(data, null, 2), 'utf8')
        bridge.refreshAgent()
        bridge.addMessage({ role: 'system', text: `  ${chalk.green('✓')} Agent model → ${chalk.bold(selected.name)} @ ${chalk.dim(selected.endpoint)}` })
      } else {
        writeOrchestratorModel(selected.name)
        bridge.addMessage({ role: 'system', text: `  ${chalk.green('✓')} Orchestrator model → ${chalk.bold(selected.name)}` })
      }
    } catch (err) {
      bridge.addMessage({ role: 'error', text: 'Failed: ' + err.message })
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

    bridge.addMessage({ role: 'assistant', text: '' })

    const reply = await streamChat(bridge.history, bearer, (partialText) => {
      if (bridge.setMessages) {
        bridge.setMessages(prev => {
          const copy = [...prev]
          if (copy.length > 0 && copy[copy.length - 1].role === 'assistant') {
            copy[copy.length - 1] = { role: 'assistant', text: partialText }
          }
          return copy
        })
      }
    })

    bridge.history.push({ role: 'assistant', content: reply })

    recordTokenAudit(Math.ceil(text.length / 4), Math.ceil(reply.length / 4), 'nehanda')

    applyDiffBlocks(reply, bridge)

  } catch (err) {
    bridge.addMessage({ role: 'error', text: err.message })
    bridge.history.pop()
  } finally {
    bridge.setLoading(false)
  }
}

render(e(App, { bridge }), { exitOnCtrlC: true })