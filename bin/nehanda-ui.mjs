#!/usr/bin/env node
// nehanda-ui — Ink terminal UI for ona-code
// Connects directly to nehanda-ml.asoba.co via the in-process ona-code engine.
// No aimee-server, no HTTP daemon, no cross-repo resolution.

if (process.stdout.isTTY) {
  process.stdout.write('\x1b[2J\x1b[3J\x1b[H')
}

import fs from 'node:fs'
import os from 'node:os'
import path from 'node:path'
import { execFile } from 'node:child_process'
import { promisify } from 'node:util'
import { fileURLToPath } from 'node:url'

// ── In-process engine imports (direct, single-repo) ──────────
import { runUserTurn } from '../lib/orchestrate.mjs'
import { loadInstructions } from '../lib/instructions.mjs'
import { modelConfigFromAgent } from '../lib/modelConfig.mjs'
import { runConfigureWizard } from '../lib/configureProvider.mjs'
import {
  TUI_SLASH_COMMANDS,
  filterSlashAutocomplete,
  parseMcpEnvParts,
} from '../lib/slashCommands.mjs'
import { applyMcpConfig, loadMcpConfig, writeGlobalMcpConfig, readGlobalMcpConfig, globalMcpConfigPath, updateMcpServerEnv } from '../lib/mcp-config.mjs'
import { getMcpServer, closeAllMcpServers, listMcpServers, mcpListResources } from '../lib/mcp.mjs'
import { invalidateMcpToolCache } from '../lib/tools.mjs'
import { saveOpenAiCompatKey, resolveOpenAiCompatKey, saveSecureCredentials, resolveZhipuApiKey } from '../lib/auth.mjs'
import { TOOL_VERBS, verbForTool, summarizeToolArgs, formatToolResultSuffix, maskSecretAnswer } from '../lib/ui.mjs'
import {
  PICKER_THRESHOLD,
  createAskIdSequencer,
  groupModelsByVendor,
  filterModelItems,
  visibleWindow,
} from '../lib/modelPicker.mjs'
import Database from 'better-sqlite3'
import { ensureLayaReady } from '../lib/laya_sidecar.mjs'

const __dirname = path.dirname(fileURLToPath(import.meta.url))
const REPO_ROOT  = path.resolve(__dirname, '..')

// All remaining UI deps live in ona-code's own node_modules
const { default: React }          = await import(`${REPO_ROOT}/node_modules/react/index.js`)
const { render, Box, Text, useApp, useInput, Static }
                                  = await import(`${REPO_ROOT}/node_modules/ink/build/index.js`)
const { default: TextInput }      = await import(`${REPO_ROOT}/node_modules/ink-text-input/build/index.js`)
const { default: chalk }          = await import(`${REPO_ROOT}/node_modules/chalk/source/index.js`)
const { marked }                  = await import(`${REPO_ROOT}/node_modules/marked/lib/marked.esm.js`)
const { default: TerminalRenderer }= await import(`${REPO_ROOT}/node_modules/marked-terminal/index.js`)

const e = React.createElement

// Per-question id source for AskPrompt React keys (HP-1a, #10). Module scope:
// ids only need to be unique for the process lifetime.
const askIdSeq = createAskIdSequencer()

// ── Asoba 3 Visual Identity ───────────────────────────────────
const BRAND = {
  accent:   '#455BF1',
  lavender: '#9D93D6',
  coral:    '#F2AEAC',
  red:      '#E20419',
  dim:      'gray',
}

// ── Config paths ─────────────────────────────────────────────
const NEHANDA_DIR   = path.join(os.homedir(), '.config', 'nehanda')
const NEHANDA_DB    = path.join(NEHANDA_DIR, 'ona-session.db')
const AIMEE_DIR     = path.join(os.homedir(), '.config', 'aimee')
const AGENTS_JSON   = path.join(AIMEE_DIR,   'agents.json')
const SPLASH_SEEN   = path.join(NEHANDA_DIR, '.splash-seen')

function hasSeenSplash() { try { return fs.existsSync(SPLASH_SEEN) } catch { return false } }
function markSplashSeen() {
  try { fs.mkdirSync(NEHANDA_DIR, { recursive: true }); fs.writeFileSync(SPLASH_SEEN, new Date().toISOString()) }
  catch {}
}

// ── Config helpers ────────────────────────────────────────────
function readAgents() {
  try { return JSON.parse(fs.readFileSync(AGENTS_JSON, 'utf8')) }
  catch { return { agents: [], default_agent: '' } }
}

/** Nehanda API key: stored in agents.json api_key or NEHANDA_API_KEY env var. */
function readApiKey() {
  const data  = readAgents()
  const agent = data.agents?.find(a => a.name === data.default_agent) || data.agents?.[0]
  return agent?.api_key || process.env.NEHANDA_API_KEY || ''
}

function writeApiKey(key) {
  const data  = readAgents()
  const agent = data.agents?.find(a => a.name === data.default_agent) || data.agents?.[0]
  if (agent) { agent.api_key = key; fs.writeFileSync(AGENTS_JSON, JSON.stringify(data, null, 2), 'utf8') }
}

// ── Session DB bootstrap ─────────────────────────────────────
fs.mkdirSync(NEHANDA_DIR, { recursive: true })
const sessionDb = new Database(NEHANDA_DB)
const { content: onaInstructionsContent } = loadInstructions(process.cwd())
sessionDb.exec(`
  CREATE TABLE IF NOT EXISTS conversations(
    id TEXT PRIMARY KEY, project_dir TEXT,
    phase TEXT DEFAULT 'idle',
    last_active TEXT DEFAULT (datetime('now'))
  );
  CREATE TABLE IF NOT EXISTS transcript_entries(
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    session_id TEXT NOT NULL, sequence INTEGER NOT NULL,
    entry_type TEXT NOT NULL, payload_json TEXT NOT NULL,
    tool_use_id TEXT, created_at TEXT DEFAULT (datetime('now'))
  );
  CREATE TABLE IF NOT EXISTS plans(
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    conversation_id TEXT NOT NULL, content TEXT,
    hash TEXT, status TEXT DEFAULT 'draft', approved_at TEXT
  );
  CREATE TABLE IF NOT EXISTS events(
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    conversation_id TEXT NOT NULL,
    event_type TEXT NOT NULL, detail TEXT,
    created_at TEXT DEFAULT (datetime('now'))
  );
  CREATE TABLE IF NOT EXISTS summaries(
    conversation_id TEXT PRIMARY KEY,
    content TEXT, word_count INTEGER,
    updated_at TEXT DEFAULT (datetime('now'))
  );
`)

// ── Markdown ─────────────────────────────────────────────────
marked.setOptions({
  renderer: new TerminalRenderer({
    code:      chalk.hex(BRAND.lavender),
    codespan:  chalk.hex(BRAND.lavender),
    strong:    chalk.bold,
    em:        chalk.italic,
    heading:   chalk.bold.hex(BRAND.accent),
    hr:        () => chalk.dim('─'.repeat(50)),
    paragraph: text => text + '\n',
    link:      (href, _t, text) => `${text} ${chalk.dim.underline(href)}`,
  })
})
function renderMd(text) {
  if (!text) return ''
  try { return marked(text).replace(/\n{3,}/g, '\n\n').trimEnd() }
  catch { return text }
}

// ── Constants ────────────────────────────────────────────────
const SPINNER_FRAMES = ['⠋','⠙','⠹','⠸','⠼','⠴','⠦','⠧','⠇','⠏']
const SLASH_COMMANDS = TUI_SLASH_COMMANDS

// ── Asoba lock-up (compact) ───────────────────────────────────
function AsobaMark() {
  return e(Box, { flexDirection: 'column', alignItems: 'center' },
    e(Text, {}, e(Text, { color: BRAND.accent }, '▐▐▐ ')),
    e(Text, { bold: true }, 'N E H A N D A'),
    e(Text, { dimColor: true }, 'by Asoba'),
  )
}

const ASOBA_BADGE = `                                                  
                  ............'.                  
              '....................'              
           ............................           
         ................................         
       ....................................       
      ......................................      
     ........................................     
   \`.................;(LwwU{,.................    
   ...............I&$$$$$$$$$$a^...............   
  .............."W$$$$@$@@$@$$$$Y...............  
  .............,@$$$$$$$$$$$@$@$$O..............  
 '.............W$$$$$$t...^M$$$$$@;.............  
 '.............$$$@$$/.....'@$$$$@{.............  
 ..............@$$$$@u......@$$@$$).............  
 ..............k$$$$@@q:..:.B$$$$$).............  
  .............\`8$$$@$$$$$W'B$$$$$).............  
  ..............'q$@$$$$$$W'B$@$$$).............  
   ...............\`d$@$$@$W'B@$$$$(............   
   ..................'I]|[l....................   
    .........................................'    
     .......................................'     
      .....................................       
        ..................................        
          ..............................          
             ........................             
                ..................                
                        \`\``

const BADGE_DOT = new Set([' ','.', "'", '`',',',':',';','^'])
function BadgeLine({ line }) {
  const runs = []
  for (const ch of line) {
    const cls = BADGE_DOT.has(ch) ? 'dot' : 'glyph'
    const last = runs[runs.length - 1]
    if (last && last.cls === cls) last.text += ch
    else runs.push({ cls, text: ch })
  }
  return e(Text, {}, ...runs.map((r, i) => e(Text, {
    key: String(i), color: r.cls === 'glyph' ? BRAND.accent : undefined,
    bold: r.cls === 'glyph', dimColor: r.cls === 'dot',
  }, r.text)))
}
function AsobaBadge() {
  const lines = ASOBA_BADGE.split('\n')
  return e(Box, { flexDirection: 'column', alignItems: 'center' },
    ...lines.map((l, i) => e(BadgeLine, { key: String(i), line: l })))
}
function SplashScreen({ onDone }) {
  useInput(() => onDone())
  React.useEffect(() => { const t = setTimeout(onDone, 1400); return () => clearTimeout(t) }, [onDone])
  let cols = 80; try { cols = process.stdout.columns || 80 } catch {}
  const bw = Math.max(...ASOBA_BADGE.split('\n').map(l => l.length))
  return e(Box, { flexDirection: 'column', alignItems: 'center', marginY: 1 },
    cols >= bw + 4 ? e(AsobaBadge, {}) : e(AsobaMark, {}),
    e(Box, { marginTop: 1 }, e(Text, { bold: true, color: BRAND.accent }, 'NEHANDA')),
    e(Text, { dimColor: true }, 'by Asoba'),
    e(Box, { marginTop: 1 }, e(Text, { dimColor: true }, 'press any key to continue…')),
  )
}
function WelcomeBanner({ model, endpoint }) {
  let cols = 80; try { cols = process.stdout.columns || 80 } catch {}
  const bw = Math.min(cols - 2, 80)
  const lw = Math.floor(bw * 0.42), rw = bw - lw - 3
  return e(Box, { flexDirection: 'column', borderStyle: 'single', borderColor: BRAND.accent, width: bw, marginBottom: 1 },
    e(Box, { flexDirection: 'row', paddingX: 1 },
      e(Box, { flexDirection: 'column', width: lw, alignItems: 'center', paddingY: 1 },
        e(AsobaMark, {}), e(Text, { dimColor: true }, model || '—')),
      e(Box, { width: 1, borderStyle: 'single', borderColor: BRAND.accent, borderTop: false, borderBottom: false, borderRight: false, borderLeft: true }),
      e(Box, { flexDirection: 'column', width: rw, paddingLeft: 1, paddingY: 1 },
        e(Text, { bold: true, color: BRAND.accent }, 'Configuration'),
        e(Box, { marginTop: 1, flexDirection: 'column' },
          e(Text, {}, e(Text, { dimColor: true }, 'Model:    '), e(Text, { color: BRAND.lavender, bold: true }, model || '—')),
          e(Text, {}, e(Text, { dimColor: true }, 'Endpoint: '), e(Text, { dimColor: true }, endpoint || '—')),
        ),
        e(Box, { marginTop: 1 }, e(Text, { dimColor: true }, 'Type /help for commands')),
      ),
    ),
  )
}

// ── Message view ─────────────────────────────────────────────
function MessageView({ msg }) {
  if (msg.role === 'user')
    return e(Box, { flexDirection: 'row' }, e(Text, { bold: true, color: BRAND.accent }, '❯ '),
      e(Box, { flexShrink: 1 }, e(Text, { wrap: 'wrap' }, msg.text)))
  if (msg.role === 'assistant') {
    if (!msg.text) return msg.cancelled
      ? e(Box, { flexDirection: 'row' }, e(Text, { dimColor: true }, '● '), e(Text, { dimColor: true, italic: true }, '(cancelled)'))
      : null
    return e(Box, { flexDirection: 'row' }, e(Text, { dimColor: true }, '● '),
      e(Box, { flexDirection: 'column', flexShrink: 1 },
        e(Text, {}, renderMd(msg.text)),
        msg.cancelled ? e(Text, { dimColor: true, italic: true }, '⏹ cancelled') : null))
  }
  if (msg.role === 'system')
    return e(Box, { marginLeft: 1 }, e(Text, { dimColor: true }, msg.text))
  if (msg.role === 'error')
    return e(Box, { marginLeft: 1 }, e(Text, { color: BRAND.red }, '✗ ' + msg.text))
  return null
}
function Spinner({ label }) {
  const [frame, setFrame] = React.useState(0)
  React.useEffect(() => { const t = setInterval(() => setFrame(f => f + 1), 80); return () => clearInterval(t) }, [])
  // Generic labels rotate plain-English verbs; specific labels (e.g. "Reading ui.mjs") stay static.
  const verb = (!label || label === 'Thinking')
    ? TOOL_VERBS[Math.floor(frame / 5) % TOOL_VERBS.length]
    : label
  return e(Box, { marginLeft: 1 }, e(Text, { color: BRAND.accent }, SPINNER_FRAMES[frame % SPINNER_FRAMES.length] + ' '), e(Text, { dimColor: true }, verb + '…'))
}
function SlashMenu({ filter }) {
  const matches = filterSlashAutocomplete(filter)
  if (!matches.length) return null
  const pad = Math.min(36, Math.max(...matches.map(c => c.name.length), 12) + 1)
  return e(Box, { flexDirection: 'column', marginLeft: 2 },
    ...matches.map((c, i) => e(Box, { key: String(i) },
      e(Text, { color: BRAND.accent }, c.name.padEnd(pad)), e(Text, { dimColor: true }, c.desc))))
}
function InputArea({ onSubmit, isLoading }) {
  const [value, setValue] = React.useState('')
  // Show slash menu for top-level (`/m`) and while typing known parents (`/mcp `, `/key o`)
  const showMenu = value.startsWith('/') && (
    !value.includes(' ')
    || /^\/(mcp|key)(\s|$)/i.test(value)
  )
  const handleSubmit = React.useCallback(v => { if (!v.trim()) return; setValue(''); onSubmit(v.trim()) }, [onSubmit])
  if (isLoading) return e(Box, { paddingX: 1 }, e(Text, { dimColor: true }, 'esc to cancel'))
  return e(Box, { flexDirection: 'column' },
    showMenu ? e(SlashMenu, { filter: value }) : null,
    e(Box, { paddingX: 1 }, e(Text, { bold: true, color: BRAND.accent }, '❯ '),
      e(TextInput, { value, onChange: setValue, onSubmit: handleSubmit })))
}
function AskPrompt({ question, onAnswer, onCancel }) {
  const [value, setValue] = React.useState('')
  // Defense-in-depth for HP-1a (#10): if this instance ever re-renders with a
  // new question (fiber reuse), the typed value must not carry over.
  React.useEffect(() => { setValue('') }, [question])
  useInput((_input, key) => { if (key.escape) onCancel() })
  return e(Box, { flexDirection: 'column' },
    e(Box, { marginLeft: 1 }, e(Text, { dimColor: true }, question)),
    e(Box, { paddingX: 1 }, e(Text, { color: BRAND.accent }, '❯ '),
      e(TextInput, { value, onChange: setValue, onSubmit: React.useCallback(v => onAnswer(v.trim()), [onAnswer]) })),
    e(Box, { marginLeft: 1 }, e(Text, { dimColor: true }, 'Esc cancel')))
}
// ── Model Picker (large lists, #10 / HP-2) ───────────────────
// Three views: vendor GROUPS → windowed BROWSE within a vendor →
// type-to-filter SEARCH across every discovered model.
// Digits jump rows, '/' opens search, Esc steps back.
function ModelPicker({ title, items, onSelect, onCancel }) {
  const groups = React.useMemo(() => groupModelsByVendor(items), [items])
  const [view, setView] = React.useState('groups')   // 'groups' | 'browse' | 'search'
  const [groupIdx, setGroupIdx] = React.useState(0)
  const [browseIdx, setBrowseIdx] = React.useState(0)
  const [filter, setFilter] = React.useState('')
  const [searchIdx, setSearchIdx] = React.useState(0)

  const browseModels = view === 'browse' && groups[groupIdx] ? groups[groupIdx].models : []
  const searchResults = React.useMemo(() => filterModelItems(items, filter), [items, filter])
  const prevView = React.useRef('groups')

  const jump = (setter, list, n) => {
    if (!isNaN(n) && n >= 1 && n <= list.length) setter(n - 1)
  }

  useInput((input, key) => {
    if (view === 'search') {
      // TextInput owns printable chars + Enter (its onSubmit); arrows pass through.
      if (key.escape) {
        if (filter) { setFilter(''); setSearchIdx(0) }
        else { setView(prevView.current); setSearchIdx(0) }
      } else if (key.upArrow)   { setSearchIdx(i => (i - 1 + searchResults.length) % searchResults.length) }
      else if (key.downArrow) { setSearchIdx(i => (i + 1) % searchResults.length) }
      return
    }
    if (key.escape) {
      if (view === 'browse') setView('groups')
      else onCancel()
      return
    }
    if (input === '/') { prevView.current = view; setView('search'); setFilter(''); setSearchIdx(0); return }
    if (view === 'groups') {
      if (key.upArrow)   { setGroupIdx(i => (i - 1 + groups.length) % groups.length); return }
      if (key.downArrow) { setGroupIdx(i => (i + 1) % groups.length); return }
      if (key.return) {
        const g = groups[groupIdx]
        if (!g) return
        if (g.isCustom) { onSelect(g.models[0]); return }
        setBrowseIdx(0); setView('browse')
        return
      }
      jump(setGroupIdx, groups, parseInt(input, 10))
    } else if (view === 'browse') {
      if (key.upArrow)   { setBrowseIdx(i => (i - 1 + browseModels.length) % browseModels.length); return }
      if (key.downArrow) { setBrowseIdx(i => (i + 1) % browseModels.length); return }
      if (key.return)    { const m = browseModels[browseIdx]; if (m) onSelect(m); return }
      const win = visibleWindow(browseModels, browseIdx)
      const shown = win.visible
      const n = parseInt(input, 10)
      if (!isNaN(n) && n >= 1 && n <= shown.length) {
        const item = shown[n - 1]
        const abs = browseModels.indexOf(item)
        if (abs !== -1) setBrowseIdx(abs)
      }
    }
  })

  const row = (item, i, cursor) => e(Box, { key: String(item.id ?? i), flexDirection: 'column' },
    e(Box, {}, e(Text, { color: cursor ? BRAND.accent : undefined }, cursor ? '❯ ' : '  '),
      e(Text, { color: BRAND.lavender }, `${i + 1}. `),
      e(Text, { bold: true, color: cursor ? BRAND.accent : undefined }, item.label)),
    e(Box, { marginLeft: 5 }, e(Text, { dimColor: true }, item.desc)))
  const hiddenHints = win => e(Box, { key: 'hints', flexDirection: 'column' },
    win.hiddenAbove ? e(Text, { dimColor: true }, `  ↑ ${win.hiddenAbove} more above`) : null,
    win.hiddenBelow ? e(Text, { dimColor: true }, `  ↓ ${win.hiddenBelow} more below`) : null)

  let body
  if (view === 'groups') {
    body = e(Box, { marginTop: 1, flexDirection: 'column' },
      ...groups.map((g, i) => row(g, i, i === groupIdx)),
      e(Box, { marginTop: 1 }, e(Text, { dimColor: true },
        `↑↓/1–${Math.min(9, groups.length)} vendor · Enter open · / search all · Esc cancel`)))
  } else if (view === 'browse') {
    const win = visibleWindow(browseModels, browseIdx)
    body = e(Box, { marginTop: 1, flexDirection: 'column' },
      e(Text, { dimColor: true }, `${groups[groupIdx].label} — ${browseModels.length} models`),
      ...win.visible.map((m, i) => row(m, win.start + i, win.start + i === browseIdx)),
      hiddenHints(win),
      e(Box, { marginTop: 1 }, e(Text, { dimColor: true },
        '↑↓ navigate · Enter select · / search all · Esc back')))
  } else {
    const win = visibleWindow(searchResults, searchIdx)
    const handleSearchSubmit = v => {
      const item = searchResults[searchIdx]
      if (item) onSelect(item)
    }
    body = e(Box, { marginTop: 1, flexDirection: 'column' },
      e(Box, { paddingX: 1 }, e(Text, { color: BRAND.accent }, '/'),
        e(TextInput, { value: filter, onChange: v => { setFilter(v); setSearchIdx(0) }, onSubmit: handleSearchSubmit })),
      searchResults.length
        ? e(Box, { marginTop: 1, flexDirection: 'column' },
            ...win.visible.map((m, i) => row(m, win.start + i, win.start + i === searchIdx)),
            hiddenHints(win))
        : e(Text, { dimColor: true }, '  (no matches)'),
      e(Box, { marginTop: 1 }, e(Text, { dimColor: true },
        'type to filter · ↑↓ select · Enter confirm · Esc clear/back')))
  }

  return e(Box, { flexDirection: 'column', marginLeft: 1 },
    e(Text, { bold: true, color: BRAND.accent }, title),
    body)
}

// Wrapper (no hooks): large catalogs (e.g. OpenRouter's hundreds of models)
// switch to the grouped/filterable picker instead of a flat scroll list (HP-2, #10).
function SelectMenu({ title, items, onSelect, onCancel }) {
  if (items.length > PICKER_THRESHOLD) {
    return e(ModelPicker, { title, items, onSelect, onCancel })
  }
  return e(SelectMenuFlat, { title, items, onSelect, onCancel })
}

function SelectMenuFlat({ title, items, onSelect, onCancel }) {
  const [idx, setIdx] = React.useState(0)
  useInput((input, key) => {
    if (key.escape)    { onCancel(); return }
    if (key.upArrow)   { setIdx(i => (i - 1 + items.length) % items.length); return }
    if (key.downArrow) { setIdx(i => (i + 1) % items.length); return }
    if (key.return)    { onSelect(items[idx]); return }
    const n = parseInt(input, 10)
    if (!isNaN(n) && n >= 1 && n <= items.length) onSelect(items[n - 1])
  })
  return e(Box, { flexDirection: 'column', marginLeft: 1 },
    e(Text, { bold: true, color: BRAND.accent }, title),
    e(Box, { marginTop: 1, flexDirection: 'column' },
      ...items.map((item, i) => {
        const active = i === idx
        return e(Box, { key: String(i), flexDirection: 'column' },
          e(Box, {}, e(Text, { color: active ? BRAND.accent : undefined }, active ? '❯ ' : '  '),
            e(Text, { color: BRAND.lavender }, `${i + 1}. `),
            e(Text, { bold: true, color: active ? BRAND.accent : undefined }, item.label)),
          e(Box, { marginLeft: 5 }, e(Text, { dimColor: true }, item.desc)))
      })),
    e(Box, { marginTop: 1 }, e(Text, { dimColor: true },
      '↑↓ navigate · Enter select' + (items.length <= 9 ? ` · 1–${items.length} jump` : '') + ' · Esc cancel')))
}

// ── Block Confirmation Menu (Gap 3) ──────────────────────────
// Raised when bashguard or a safety-checker script blocks a command.
// Resolves the Promise with 'approve' | 'sandbox' | 'cancel'.
function BlockConfirmMenu({ payload, onDecide }) {
  useInput((input, key) => {
    const ch = input.toLowerCase()
    if (key.escape || ch === 'c') { onDecide('cancel'); return }
    if (ch === 'a') { onDecide('approve'); return }
    if (ch === 's') { onDecide('sandbox'); return }
  })

  const riskColor = payload.risk_score === 'high' ? BRAND.red : BRAND.coral
  const remediation = (payload.proposed_remediation || []).join(', ')

  return e(Box, { flexDirection: 'column', borderStyle: 'single', borderColor: BRAND.red, marginY: 1 },
    e(Box, { paddingX: 1, paddingY: 0 },
      e(Text, { bold: true, color: BRAND.red }, '⛔ Command Blocked')),
    e(Box, { paddingX: 1 },
      e(Text, { dimColor: true }, 'Reason:      '),
      e(Text, { wrap: 'wrap' }, payload.reason || '(unknown)')),
    e(Box, { paddingX: 1 },
      e(Text, { dimColor: true }, 'Risk:        '),
      e(Text, { color: riskColor, bold: true }, (payload.risk_score || 'unknown').toUpperCase())),
    remediation
      ? e(Box, { paddingX: 1 },
          e(Text, { dimColor: true }, 'Remediation: '),
          e(Text, { dimColor: true }, remediation))
      : null,
    e(Box, { paddingX: 1, marginTop: 1 },
      e(Text, { bold: true, color: BRAND.accent }, '[A]'),
      e(Text, {}, ' Approve for Session  '),
      e(Text, { bold: true, color: BRAND.lavender }, '[S]'),
      e(Text, {}, ' Run in Laya Sandbox  '),
      e(Text, { bold: true, color: BRAND.dim }, '[C]'),
      e(Text, {}, ' Cancel')))
}

function TokenFooter({ calls, model }) {
  const cols = process.stdout.columns || 80
  return e(Box, { flexDirection: 'column' },
    e(Text, { dimColor: true }, '─'.repeat(cols)),
    e(Box, { flexDirection: 'row', paddingX: 1, justifyContent: 'space-between' },
      e(Text, { dimColor: true }, `calls ${calls}`),
      e(Text, {}, e(Text, { dimColor: true }, 'model: '), e(Text, { color: BRAND.lavender }, model || '—')),
      e(Text, { dimColor: true }, '? /help  ^C exit')))
}

// ── Main App ──────────────────────────────────────────────────
function App({ bridge }) {
  const { exit } = useApp()
  const [showSplash, setShowSplash] = React.useState(() => !hasSeenSplash())
  const [finalized,  setFinalized]  = React.useState([])
  const [streaming,  setStreaming]  = React.useState(null)
  const [clearGen,   setClearGen]   = React.useState(0)
  const [isLoading,  setIsLoading]  = React.useState(false)
  const [askState,   setAskState]   = React.useState(null)
  const [selectState, setSelectState] = React.useState(null)
  const [blockState,  setBlockState]  = React.useState(null)
  const [callCount,  setCallCount]  = React.useState(0)
  const [spinLabel,  setSpinLabel]  = React.useState(null)
  const [modelInfo,  setModelInfo]  = React.useState(() => {
    const data = readAgents()
    const ag = data.agents?.find(a => a.name === data.default_agent) || data.agents?.[0]
    return { model: ag?.model || 'nehanda-rag-synthesis-27b', endpoint: ag?.endpoint || 'https://nehanda-ml.asoba.co/v1' }
  })
  const lastCtrlCRef = React.useRef(0)
  const bannerRef    = React.useRef({ kind: 'banner' })
  const historyItems = React.useMemo(() => [bannerRef.current, ...finalized], [finalized])

  React.useEffect(() => {
    bridge.addMessage     = msg  => setFinalized(p => [...p, msg])
    bridge.beginAssistant = ()   => setStreaming({ text: '' })
    bridge.updateAssistant= text => setStreaming({ text })
    bridge.discardAssistant=()   => setStreaming(null)
    bridge.endAssistant   = msg  => { setFinalized(p => [...p, msg]); setStreaming(null); setCallCount(c => c + 1) }
    bridge.clearMessages  = ()   => { setFinalized([]); setStreaming(null); setClearGen(g => g + 1) }
    bridge.setLoading     = v    => setIsLoading(v)
    bridge.updateSpinner  = text => setSpinLabel(text)
    // HP-1a (#10): unique id per question — AskPrompt is keyed by it, so every
    // new wizard question remounts with an EMPTY input (no key carry-over).
    // opts.secret → the persisted transcript line is masked (HP-1b).
    bridge.ask    = (q, opts) => new Promise(resolve =>
      setAskState({ id: askIdSeq.next(), question: q, secret: !!(opts && opts.secret), resolve }))
    bridge.select = (t,i) => new Promise(resolve => setSelectState({ title: t, items: i, resolve }))
    bridge.confirmBlock = payload => new Promise(resolve => setBlockState({ payload, resolve }))
    bridge.exit   = ()    => { exit() }
    bridge.refreshModel = () => {
      const data = readAgents()
      const ag = data.agents?.find(a => a.name === data.default_agent) || data.agents?.[0]
      setModelInfo({ model: ag?.model || 'nehanda-rag-synthesis-27b', endpoint: ag?.endpoint || 'https://nehanda-ml.asoba.co/v1' })
    }
  }, [bridge, exit])

  const handleAnswer = React.useCallback(answer => {
    // HP-1b (#10): secret answers persist masked to transcript history; the
    // wizard still receives the raw answer via resolve().
    const shown = askState?.secret ? maskSecretAnswer(answer) : answer
    if (askState?.resolve) { setFinalized(p => [...p, { role: 'system', text: `${askState.question} ${shown}`.trim() }]); askState.resolve(answer) }
    setAskState(null)
  }, [askState])
  const handleAskCancel = React.useCallback(() => { askState?.resolve(null); setAskState(null) }, [askState])
  const handleSelect    = React.useCallback(item => {
    if (selectState?.resolve) { setFinalized(p => [...p, { role: 'system', text: `  Selected: ${item.label}` }]); selectState.resolve(item) }
    setSelectState(null)
  }, [selectState])
  const handleSelectCancel = React.useCallback(() => { selectState?.resolve(null); setSelectState(null) }, [selectState])
  const handleBlock = React.useCallback(decision => {
    if (blockState?.resolve) {
      const label = decision === 'approve' ? '✓ Approved for session' : decision === 'sandbox' ? '⚙ Running in sandbox' : '✗ Cancelled'
      setFinalized(p => [...p, { role: 'system', text: `  ${label}` }])
      blockState.resolve(decision)
    }
    setBlockState(null)
  }, [blockState])

  useInput((input, key) => {
    if (key.escape) { if (isLoading && bridge.abortCurrent) bridge.abortCurrent(); return }
    if (key.ctrl && input === 'c') {
      if (isLoading && bridge.abortCurrent) { bridge.abortCurrent(); return }
      const now = Date.now()
      if (now - lastCtrlCRef.current < 1200) { exit(); process.exit(0) }
      lastCtrlCRef.current = now
      bridge.addMessage({ role: 'system', text: '  (press Ctrl+C again to exit)' })
    }
  })

  if (showSplash)
    return e(SplashScreen, { onDone: () => { markSplashSeen(); setShowSplash(false) } })

  return e(Box, { flexDirection: 'column' },
    e(Static, { items: historyItems, key: clearGen },
      (item, i) => item.kind === 'banner'
        ? e(WelcomeBanner, { key: 'banner', model: modelInfo.model, endpoint: modelInfo.endpoint })
        : e(MessageView,   { key: String(i), msg: item })),
    streaming ? e(MessageView, { msg: { role: 'assistant', ...streaming } }) : null,
    (isLoading && (!streaming || !streaming.text)) || spinLabel ? e(Spinner, { label: spinLabel || undefined }) : null,
    askState
      ? e(AskPrompt, { key: askState.id, question: askState.question, onAnswer: handleAnswer, onCancel: handleAskCancel })
      : blockState
        ? e(BlockConfirmMenu, { payload: blockState.payload, onDecide: handleBlock })
        : selectState
          ? e(SelectMenu, { title: selectState.title, items: selectState.items, onSelect: handleSelect, onCancel: handleSelectCancel })
          : e(InputArea, { onSubmit: bridge.onSubmit, isLoading }),
    e(TokenFooter, { calls: callCount, model: modelInfo.model }))
}

// ── Diff executor ─────────────────────────────────────────────
function applyDiffBlocks(text, bridge) {
  const re = /^(.+?\.(?:\w+))\s*\n<<<<<<< SEARCH\n([\s\S]*?)\n?=======\n([\s\S]*?)\n?>>>>>>> REPLACE/gm
  let match
  while ((match = re.exec(text)) !== null) {
    const [, filepath, search, replace] = match
    try {
      const abs = path.isAbsolute(filepath) ? filepath : path.resolve(process.cwd(), filepath)
      if (!fs.existsSync(abs)) { bridge.addMessage({ role: 'error', text: `✗ File not found: ${filepath}` }); continue }
      const content = fs.readFileSync(abs, 'utf-8')
      if (!content.includes(search)) { bridge.addMessage({ role: 'error', text: `✗ Search block not found: ${filepath}` }); continue }
      fs.writeFileSync(abs, content.replace(search, replace), 'utf-8')
      bridge.addMessage({ role: 'system', text: `  ${chalk.green('✓')} Applied diff to ${chalk.bold(filepath)}` })
    } catch (err) {
      bridge.addMessage({ role: 'error', text: `✗ ${filepath}: ${err.message}` })
    }
  }
}

// ── Thinking trace stripper ───────────────────────────────────
function stripThink(text) {
  return (text || '')
    .replace(/<think>[\s\S]*?<\/think>/g, '')
    .replace(/<think>[\s\S]*/g, '')
    .trim()
}

/**
 * UI adapter for Goose-style configure wizard (#8).
 * Persists agents.json + runtimeSettings.model_config atomically at the end.
 */
async function runUiConfigureWizard(bridge, opts = {}) {
  try {
    const result = await runConfigureWizard({
      ask: (q, opts) => bridge.ask(q, opts),
      select: (title, items) => bridge.select(title, items),
      notify: msg => bridge.addMessage({ role: 'system', text: `  ${msg}` }),
      resolveExistingKey: async (preset) => {
        if (preset.keySlot === 'nehanda') return readApiKey()
        if (preset.keySlot === 'zhipu') return resolveZhipuApiKey()
        if (preset.keySlot === 'openai_compat') return resolveOpenAiCompatKey()
        return ''
      },
      saveKey: async ({ slot, key }) => {
        if (slot === 'nehanda') writeApiKey(key)
        else if (slot === 'zhipu') saveSecureCredentials({ zhipuApiKey: key })
        else if (slot === 'openai_compat') saveOpenAiCompatKey(key)
      },
      persist: async (cfg) => {
        const data = readAgents()
        const ag = data.agents?.find(a => a.name === data.default_agent) || data.agents?.[0]
        if (ag) {
          ag.model = cfg.model
          ag.endpoint = cfg.endpoint
          if (cfg.provider === 'nehanda' && cfg.apiKey) ag.api_key = cfg.apiKey
        }
        fs.writeFileSync(AGENTS_JSON, JSON.stringify(data, null, 2), 'utf8')
        runtimeSettings.model_config = cfg.model_config || modelConfigFromAgent(
          { model: cfg.model, endpoint: cfg.endpoint },
          { apiKey: cfg.apiKey || readApiKey() },
        )
      },
    }, opts)

    if (!result) {
      bridge.addMessage({ role: 'system', text: '  Configure cancelled.' })
      return
    }
    bridge.refreshModel()
    bridge.addMessage({ role: 'system', text: [
      `  ${chalk.green('✓')} Provider → ${chalk.bold(result.provider)}`,
      `  ${chalk.green('✓')} Host     → ${chalk.dim(result.endpoint)}`,
      `  ${chalk.green('✓')} Model    → ${chalk.bold(result.model)}`,
    ].join('\n') })
  } catch (err) {
    bridge.addMessage({ role: 'error', text: 'Configure failed: ' + err.message })
  }
}

// ── Command handler ───────────────────────────────────────────
async function handleCommand(cmd, bridge) {
  const [name] = cmd.split(/\s+/)

  if (name === '/help') {
    const pad = Math.max(...TUI_SLASH_COMMANDS.map(c => c.name.length), 12) + 2
    const body = TUI_SLASH_COMMANDS.map(c =>
      `  ${chalk.hex(BRAND.accent)(c.name.padEnd(pad))}${chalk.dim(c.desc)}`
    ).join('\n')
    bridge.addMessage({ role: 'system', text: `\nCommands:\n\n${body}\n` })
    return
  }
  if (name === '/config') {
    const data = readAgents()
    const ag   = data.agents?.find(a => a.name === data.default_agent) || data.agents?.[0]
    const mc   = runtimeSettings?.model_config
    const compatKeySet = !!resolveOpenAiCompatKey()
    const zhipuKeySet  = !!resolveZhipuApiKey()
    bridge.addMessage({ role: 'system', text: [
      '', `  Provider:          ${chalk.hex(BRAND.accent)(mc?.provider || '—')}`,
      `  Model:             ${chalk.hex(BRAND.accent)(ag?.model || mc?.custom_model_name || '—')}`,
      `  Endpoint:          ${chalk.dim(ag?.endpoint || mc?.base_url || '—')}`,
      `  Nehanda key:       ${readApiKey() ? chalk.green('set') : chalk.hex(BRAND.red)('not set')}`,
      `  OpenAI-compat key: ${compatKeySet ? chalk.green('set') : chalk.dim('not set')}`,
      `  Zhipu key:         ${zhipuKeySet  ? chalk.green('set') : chalk.dim('not set')}`,
      `  ${chalk.dim('Use /model to configure provider, key, host, and model in one flow.')}`, '',
    ].join('\n') })
    return
  }
  if (name === '/clear') {
    bridge.clearMessages()
    bridge.addMessage({ role: 'system', text: '  Conversation cleared.' })
    bridge.convId = `nehanda-${Date.now()}`   // new conversation ID resets transcript
    return
  }
  if (name === '/retry') {
    if (!bridge.lastFailedText) { bridge.addMessage({ role: 'system', text: '  Nothing to retry.' }); return }
    const t = bridge.lastFailedText; bridge.lastFailedText = null; await bridge.onSubmit(t); return
  }
  if (name === '/exit' || name === '/quit') { bridge.exit(); process.exit(0) }
  if (name === '/key') {
    // /key [openrouter|zhipu|openai|nehanda] [value] — save key then continue configure wizard
    const parts = cmd.trim().split(/\s+/)
    const sub   = (parts[1] || '').toLowerCase()
    const KEY_TO_PRESET = {
      openrouter: 'openrouter',
      openai: 'openai_compatible',
      zhipu: 'zhipu',
      nehanda: 'nehanda',
    }

    if (!sub || !KEY_TO_PRESET[sub]) {
      const answer = await bridge.ask('New Nehanda API key:', { secret: true })
      if (!answer) return
      try {
        writeApiKey(answer)
        bridge.addMessage({ role: 'system', text: `  ${chalk.green('✓')} Nehanda API key saved — continuing configure…` })
        await runUiConfigureWizard(bridge, { presetId: 'nehanda', prefilledKey: answer })
      } catch (err) { bridge.addMessage({ role: 'error', text: 'Failed: ' + err.message }) }
      return
    }

    const presetId = KEY_TO_PRESET[sub]
    const inlineValue = parts.slice(2).join(' ').trim()
    const answer = inlineValue || await bridge.ask(`New ${sub} API key:`, { secret: true })
    if (!answer) return

    try {
      if (sub === 'zhipu') saveSecureCredentials({ zhipuApiKey: answer })
      else if (sub === 'nehanda') writeApiKey(answer)
      else saveOpenAiCompatKey(answer)
      bridge.addMessage({ role: 'system', text: `  ${chalk.green('✓')} ${sub} API key saved — continuing configure…` })
      await runUiConfigureWizard(bridge, { presetId, prefilledKey: answer })
    } catch (err) { bridge.addMessage({ role: 'error', text: 'Failed: ' + err.message }) }
    return
  }
  if (name === '/model') {
    await runUiConfigureWizard(bridge)
    return
  }
  if (name === '/mcp') {
    const parts = cmd.trim().split(/\s+/)
    const sub = parts[1] || ''
    const servers = runtimeSettings.mcp_servers || {}
    const serverNames = Object.keys(servers)

    if (!sub || sub === 'help') {
      const pad = 40
      const row = (n, d) => `    ${chalk.hex(BRAND.accent)(n.padEnd(pad))}${chalk.dim(d)}`
      const lines = [
        '',
        `  ${chalk.bold('MCP Server & Tool Commands:')}`,
        row('/mcp list', 'List tools from configured MCP servers'),
        row('/mcp status', 'Show configured servers'),
        row('/mcp reload [server]', 'Reload mcp.json into this session'),
        row('/mcp add <name> <command> [args…]', 'Register a server'),
        row('/mcp env', 'Show env vars for every server (values masked)'),
        row('/mcp env <server>', 'Show env vars for one server'),
        row('/mcp env <server> <NAME> <VALUE>', 'Set env var  e.g. /mcp env jupyter JUPYTER_TOKEN sk-…'),
        row('/mcp env unset <server> <NAME>', 'Remove an env var'),
        '',
        `  ${chalk.dim(`Configured servers: ${serverNames.length ? serverNames.join(', ') : 'none'}`)}`,
        `  ${chalk.dim('Tip: Run /mcp list to inspect tools injected into the assistant.')}`,
        '',
      ]
      bridge.addMessage({ role: 'system', text: lines.join('\n') })
      return
    }

    if (sub === 'status') {
      if (!serverNames.length) {
        bridge.addMessage({ role: 'system', text: '  No MCP servers configured.\n  Add servers to mcp.json in your project root or ~/.config/nehanda/mcp.json' })
        return
      }
      const lines = ['\n  MCP Servers:']
      for (const name of serverNames) {
        const cfg = servers[name]
        lines.push(`  ${chalk.hex(BRAND.accent)(name.padEnd(20))} ${chalk.dim(cfg.command + ' ' + (cfg.args || []).join(' '))}`)
      }
      lines.push('')
      lines.push(`  ${chalk.dim('Tip: Run /mcp list to view tools provided by these servers.')}\n`)
      bridge.addMessage({ role: 'system', text: lines.join('\n') })
      return
    }

    if (sub === 'list') {
      if (!serverNames.length) {
        bridge.addMessage({ role: 'system', text: '  No MCP servers configured.' })
        return
      }
      bridge.addMessage({ role: 'system', text: '  Discovering MCP tools…' })
      const lines = []
      for (const srvName of serverNames) {
        const cfg = servers[srvName]
        try {
          const srv = getMcpServer(srvName, cfg)
          const resp = await srv.request('tools/list', {})
          const tools = resp.result?.tools || []
          if (resp.error) {
            lines.push(`  ${chalk.hex(BRAND.red)('✗')} ${srvName}: ${resp.error.message}`)
          } else if (!tools.length) {
            lines.push(`  ${chalk.dim(srvName)}: (no tools)`)
          } else {
            lines.push(`  ${chalk.hex(BRAND.accent)(srvName)} (${tools.length} tool${tools.length === 1 ? '' : 's'}):`)
            for (const t of tools) {
              lines.push(`    ${chalk.hex(BRAND.lavender)(t.name.padEnd(30))} ${chalk.dim(t.description || '')}`)
            }
          }
        } catch (err) {
          lines.push(`  ${chalk.hex(BRAND.red)('✗')} ${srvName}: ${err.message}`)
        }
      }
      bridge.addMessage({ role: 'system', text: '\n' + lines.join('\n') + '\n' })
      return
    }

    if (sub === 'reload') {
      const targetServer = parts[2] || null
      // Re-read mcp.json into runtimeSettings
      runtimeSettings.mcp_servers = {}
      applyMcpConfig(runtimeSettings, process.cwd())
      // Bust the tool discovery cache so next turn re-fetches
      invalidateMcpToolCache(targetServer || undefined)
      const count = Object.keys(runtimeSettings.mcp_servers).length
      bridge.addMessage({ role: 'system', text: `  ${chalk.green('✓')} MCP config reloaded. ${count} server${count === 1 ? '' : 's'} configured.` })
      return
    }

    if (sub === 'add') {
      // /mcp add <name> <command> [args...]
      const srvName = parts[2]
      const command = parts[3]
      const args = parts.slice(4)
      if (!srvName || !command) {
        bridge.addMessage({ role: 'system', text: '  Usage: /mcp add <name> <command> [args…]\n  Example: /mcp add filesystem npx -y @modelcontextprotocol/server-filesystem /path' })
        return
      }
      const existing = readGlobalMcpConfig()
      existing[srvName] = { command, args, env: {} }
      writeGlobalMcpConfig(existing)
      // Reload immediately
      runtimeSettings.mcp_servers = {}
      applyMcpConfig(runtimeSettings, process.cwd())
      invalidateMcpToolCache()
      bridge.addMessage({ role: 'system', text: `  ${chalk.green('✓')} Added ${chalk.bold(srvName)} → ${command} ${args.join(' ')}\n  Saved to ${globalMcpConfigPath()}` })
      return
    }

    if (sub === 'env') {
      const parsed = parseMcpEnvParts(parts.slice(1))

      if (parsed.action === 'usage_error') {
        bridge.addMessage({ role: 'system', text: '  ' + parsed.error.replace(/\n/g, '\n  ') })
        return
      }

      if (parsed.action === 'list_all') {
        if (!serverNames.length) {
          bridge.addMessage({ role: 'system', text: '  No MCP servers configured.' })
          return
        }
        const lines = ['\n  MCP Server Environment Variables:']
        for (const srv of serverNames) {
          const cfg = servers[srv]
          const envKeys = Object.keys(cfg.env || {})
          if (!envKeys.length) {
            lines.push(`  ${chalk.hex(BRAND.accent)(srv)}: (none)`)
          } else {
            lines.push(`  ${chalk.hex(BRAND.accent)(srv)}:`)
            for (const k of envKeys) {
              const v = cfg.env[k]
              const masked = v.length > 10 ? v.slice(0, 6) + '...' + v.slice(-4) : '********'
              lines.push(`    ${k} = ${chalk.dim(masked)}`)
            }
          }
        }
        bridge.addMessage({ role: 'system', text: lines.join('\n') + '\n' })
        return
      }

      if (parsed.action === 'list_server') {
        const cfg = servers[parsed.server]
        if (!cfg) {
          bridge.addMessage({ role: 'system', text: `  Server "${parsed.server}" not found.` })
          return
        }
        const envKeys = Object.keys(cfg.env || {})
        if (!envKeys.length) {
          bridge.addMessage({ role: 'system', text: `  ${chalk.hex(BRAND.accent)(parsed.server)}: (no env vars set)\n  Set one with: /mcp env ${parsed.server} <NAME> <VALUE>` })
        } else {
          const lines = [`  ${chalk.hex(BRAND.accent)(parsed.server)}:`]
          for (const k of envKeys) {
            const v = cfg.env[k]
            const masked = v.length > 10 ? v.slice(0, 6) + '...' + v.slice(-4) : '********'
            lines.push(`    ${k} = ${chalk.dim(masked)}`)
          }
          bridge.addMessage({ role: 'system', text: lines.join('\n') + '\n' })
        }
        return
      }

      if (parsed.action === 'set' || parsed.action === 'unset') {
        const value = parsed.action === 'unset' ? '' : parsed.value
        const result = updateMcpServerEnv(parsed.server, parsed.name, value, process.cwd())
        if (!result.ok) {
          bridge.addMessage({ role: 'system', text: `  ${chalk.hex(BRAND.red)('✗')} ${result.error}` })
          return
        }
        runtimeSettings.mcp_servers = {}
        applyMcpConfig(runtimeSettings, process.cwd())
        invalidateMcpToolCache()
        if (parsed.action === 'set') {
          bridge.addMessage({ role: 'system', text: `  ${chalk.green('✓')} Set ${chalk.bold(parsed.name)} = ******** on ${chalk.bold(parsed.server)}\n  Config file: ${result.path}` })
        } else {
          bridge.addMessage({ role: 'system', text: `  ${chalk.green('✓')} Unset ${chalk.bold(parsed.name)} on ${chalk.bold(parsed.server)}\n  Config file: ${result.path}` })
        }
        return
      }
    }

    bridge.addMessage({ role: 'system', text: '  /mcp sub-commands: status · list · reload · add · env · env unset — type /help for full list' })
    return
  }

  bridge.addMessage({ role: 'system', text: `  Unknown command: ${name}. Type /help.` })
}

// ── Entry point ───────────────────────────────────────────────
const bridge = {
  convId:          `nehanda-${Date.now()}`,
  lastFailedText:  null,
}

// ── Runtime settings — shared across turns ───────────────────
// Built once here; applyMcpConfig merges mcp.json servers in.
// Each turn rebuilds model_config from agents.json via modelConfigFromAgent
// so /model selection changes the wire model (not just the endpoint).
const _bootAgent = (() => {
  const data = readAgents()
  return data.agents?.find(a => a.name === data.default_agent) || data.agents?.[0]
})()
const runtimeSettings = {
  model_config: modelConfigFromAgent(_bootAgent, {
    apiKey: readApiKey(),
    defaultEndpoint: process.env.NEHANDA_BASE_URL || 'https://nehanda-ml.asoba.co/v1',
  }),
  permissions: { defaultMode: 'default' },
  mcp_servers: {},
}
applyMcpConfig(runtimeSettings, process.cwd())

bridge.onSubmit = async (text) => {
  bridge.addMessage({ role: 'user', text })
  if (text.startsWith('/')) { await handleCommand(text, bridge); return }

  bridge.setLoading(true)
  const controller = new AbortController()
  bridge.abortCurrent = () => controller.abort()
  let lastPartial = ''

  try {
    bridge.beginAssistant()

    const agentsData = readAgents()
    const agent      = agentsData.agents?.find(a => a.name === agentsData.default_agent) || agentsData.agents?.[0]
    const modelConfig = modelConfigFromAgent(agent, {
      apiKey: readApiKey(),
      defaultEndpoint: process.env.NEHANDA_BASE_URL || 'https://nehanda-ml.asoba.co/v1',
    })
    runtimeSettings.model_config = modelConfig

    // Ensure conversation row exists for this convId
    sessionDb.prepare(
      `INSERT OR IGNORE INTO conversations(id, project_dir, phase) VALUES (?, ?, 'idle')`
    ).run(bridge.convId, process.cwd())

    const rt = {
      sessionId:       bridge.convId,
      conversationId:  bridge.convId,
      cwd:             process.cwd(),
      runtimeDbPath:   null,
      bareMode:        false,
      // Project instructions sourced from nehanda's own loadInstructions() (Ona.md),
      // matching bin/agent.mjs. Does NOT read agents.json exec_system_prompt — that
      // field belongs to aimee and was silently reintroducing conflicting tool-call
      // syntax (<tool_call><name>...) that fights orchestrate.mjs's [TOOL_CALL] format.
      onaInstructions: onaInstructionsContent || null,
      // Esc/Ctrl+C abort — must reach fetch / Anthropic stream (issue #7).
      signal:          controller.signal,
      settings: {
        ...runtimeSettings,
        model_config: modelConfig,
      },
    }

    let fullText = '', aborted = false
    controller.signal.addEventListener('abort', () => { aborted = true })

    const toolStarts  = new Map()
    const lastToolArgs = new Map()
    const io = {
      write(chunk) {
        if (aborted) return
        fullText += chunk
        lastPartial = stripThink(fullText).trimStart()
        bridge.updateAssistant(lastPartial)
      },
      println(msg)            { if (!aborted) bridge.addMessage({ role: 'system', text: '  ' + msg }) },
      spinner: {
        start: label => bridge.updateSpinner(label),
        stop:  ()    => bridge.updateSpinner(null),
      },
      ask:                    bridge.ask,
      confirmBlock:           payload => bridge.confirmBlock(payload),
      onToolStart(name, args) {
        if (aborted) return
        lastToolArgs.set(name, args)
        toolStarts.set(name, Date.now())
        const summary = summarizeToolArgs(name, args)
        bridge.addMessage({ role: 'system', text: `  ⚙ ${name}${summary ? `(${summary})` : ''}` })
        bridge.updateSpinner(verbForTool(name) + (summary ? ' ' + summary : ''))
      },
      onToolResult(name, c, err) {
        if (aborted) return
        const preview = String(c || '').slice(0, 120).replace(/\n/g, ' ')
        const suffix = formatToolResultSuffix(name, c, { startedAt: toolStarts.get(name), args: lastToolArgs.get(name) })
        bridge.addMessage({ role: 'system', text: `  ${err ? '✗' : '✓'} ${name}: ${preview}${suffix}` })
        bridge.updateSpinner(null)
      },
    }

    await runUserTurn(sessionDb, rt, text, io)

    if (aborted) { const e = new Error('abort'); e.name = 'AbortError'; throw e }

    const reply = stripThink(fullText)
    bridge.endAssistant({ role: 'assistant', text: reply })
    applyDiffBlocks(reply, bridge)
    bridge.lastFailedText = null

  } catch (err) {
    if (err.name === 'AbortError' || err.name === 'APIUserAbortError') {
      bridge.endAssistant({ role: 'assistant', text: lastPartial, cancelled: true })
    } else {
      bridge.discardAssistant()
      bridge.addMessage({ role: 'error', text: err.message + '\n  Type /retry to resend.' })
      bridge.lastFailedText = text
    }
  } finally {
    bridge.setLoading(false)
    bridge.abortCurrent = null
  }
}

// Provision Laya venv and warm the sidecar before the first turn.
// This is mandatory — if it fails, abort with a clear error.
try {
  await ensureLayaReady()
} catch (e) {
  process.stderr.write(`[startup] Laya provisioning failed: ${e.message}\n`)
  process.stderr.write('[startup] Cannot start: Laya (System-1) is required. Fix the error above and retry.\n')
  process.exit(1)
}

render(e(App, { bridge }), { exitOnCtrlC: false })
