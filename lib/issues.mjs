import fs from 'node:fs'
import path from 'node:path'
import { spawnSync } from 'node:child_process'

// ─── Helpers ──────────────────────────────────────────────────────────────────

function slugify(title) {
  return title
    .toLowerCase()
    .replace(/[^a-z0-9]+/g, '-')
    .replace(/^-+|-+$/g, '')
    .slice(0, 60)
}

// ─── Core API ─────────────────────────────────────────────────────────────────

/** Returns the /.issues/ directory path for the given repo root, creating it if needed. */
export function getIssuesDir(cwd) {
  const dir = path.join(cwd, '.issues')
  fs.mkdirSync(dir, { recursive: true })
  return dir
}

/** Returns the next consecutive issue number by scanning existing /.issues/ files. */
export function getNextIssueNumber(cwd) {
  const dir = getIssuesDir(cwd)
  const files = fs.readdirSync(dir).filter(f => /^\d+/.test(f))
  if (files.length === 0) return 1
  const numbers = files.map(f => {
    const m = f.match(/^(\d+)/)
    return m ? parseInt(m[1], 10) : 0
  })
  return Math.max(...numbers) + 1
}

/**
 * Removes AGENTS.md phase banner lines (e.g. `[CURRENT PHASE: CREATE_ISSUE]`)
 * so they never leak into an issue artifact or a GitHub issue body.
 *
 * @param {string} text
 * @returns {string}
 */
export function stripPhaseBanners(text) {
  return String(text || '')
    .split(/\n/)
    .filter((line) => !/^\s*\[CURRENT PHASE:.*\]\s*$/i.test(line))
    .join('\n')
    .trim()
}

/**
 * Returns the first substantive line of a reply, with AGENTS.md phase banners
 * (e.g. `[CURRENT PHASE: CREATE_ISSUE]`) and markdown headings stripped.
 *
 * @param {string} text - Model reply or user request text
 * @param {string} [fallback] - Title to use when no substantive line exists
 * @returns {string}
 */
export function issueTitleFrom(text, fallback = 'SDLC workflow issue') {
  for (const line of String(text || '').split(/\n/).map((l) => l.trim()).filter(Boolean)) {
    const stripped = line.replace(/^#+\s*/, '')
    if (!stripped || /^\[CURRENT PHASE:.*\]$/i.test(stripped)) continue
    return stripped.slice(0, 120)
  }
  return fallback
}

/**
 * Publishes the issue to GitHub first. Never throws: any failure (not a git
 * repository, gh missing/unauthenticated, API error) returns `{ok:false}` so the
 * caller can fall back to the tracked local artifact under .issues/.
 *
 * @param {string} cwd - Repository root directory
 * @param {{title: string, body: string, ghBin?: string}} opts
 * @returns {{ok: true, url: string, number: number} | {ok: false, reason: string}}
 */
export function publishToGitHub(cwd, { title, body, ghBin = process.env.ONA_GH_BIN || 'gh' }) {
  try {
    const repo = spawnSync('git', ['rev-parse', '--is-inside-work-tree'], { cwd, encoding: 'utf8' })
    if (repo.status !== 0 || String(repo.stdout || '').trim() !== 'true') return { ok: false, reason: 'not a git repository' }
    const auth = spawnSync(ghBin, ['auth', 'status'], { cwd, encoding: 'utf8' })
    if (auth.status !== 0) return { ok: false, reason: 'gh unavailable or unauthenticated' }
    const created = spawnSync(ghBin, ['issue', 'create', '--title', title, '--body', body], { cwd, encoding: 'utf8', timeout: 30_000 })
    if (created.status !== 0) return { ok: false, reason: `gh issue create failed: ${String(created.stderr || '').trim()}` }
    const match = String(created.stdout || '').match(/https:\/\/github\.com\/[^\s]+\/issues\/(\d+)/)
    if (!match) return { ok: false, reason: 'gh returned no issue URL' }
    return { ok: true, url: match[0], number: Number(match[1]) }
  } catch (error) {
    return { ok: false, reason: String(error?.message || 'gh unavailable') }
  }
}

/**
 * Creates a consecutively numbered structured markdown issue in /.issues/.
 *
 * @param {string} cwd - Repository root directory
 * @param {object} opts
 * @param {string} opts.title - Short issue title
 * @param {string} opts.observed - Observed (broken) behavior
 * @param {string} opts.expected - Expected (desired) behavior
 * @param {string} opts.diagnosis - Evidence-backed root cause
 * @param {string[]} opts.acceptanceCriteria - Array of AC strings (e.g. 'AC-1: ...', 'AC-2: ...')
 * @param {{url: string, number: number}|null} [opts.github] - GitHub issue published for this record
 * @returns {{ number: number, file: string, slug: string, github: object|null }}
 */
export function createIssue(cwd, { title, observed, expected, diagnosis, acceptanceCriteria = [], github = null }) {
  const num = getNextIssueNumber(cwd)
  const numStr = String(num).padStart(4, '0')
  const slug = slugify(title)
  const filename = `${numStr}-${slug}.md`
  const dir = getIssuesDir(cwd)
  const file = path.join(dir, filename)

  const acLines = acceptanceCriteria
    .map(ac => `- [ ] ${ac}`)
    .join('\n')

  const created = new Date().toISOString()

  const content = `# Issue #${numStr}: ${title}
Status: open
${github?.url ? `GitHub: ${github.url}\n` : ''}Created: ${created}

## Observed Behavior
${observed}

## Expected Behavior
${expected}

## Evidenced Diagnosis
${diagnosis}

## Acceptance Criteria
${acLines || '- [ ] AC-1: (define acceptance criteria)'}
`

  fs.writeFileSync(file, content, 'utf8')
  return { number: num, file, slug, github }
}

/**
 * Closes an open issue by updating its status and checking all AC checkboxes.
 *
 * @param {string} cwd - Repository root directory
 * @param {number} issueNumber - The issue number to close
 * @param {{ commitHash?: string }} [opts]
 */
export function closeIssue(cwd, issueNumber, opts = {}) {
  const dir = getIssuesDir(cwd)
  const numStr = String(issueNumber).padStart(4, '0')
  const padded = new RegExp(`^0*${issueNumber}-.*\\.md$`)
  const files = fs.readdirSync(dir).filter(f => padded.test(f) || f.startsWith(numStr + '-'))
  if (files.length === 0) {
    throw new Error(`Issue #${issueNumber} not found in ${dir}`)
  }
  const file = path.join(dir, files[0])
  let content = fs.readFileSync(file, 'utf8')

  // Update status
  content = content.replace(/^Status: open$/m, 'Status: closed')

  // Optionally record closed timestamp
  if (opts.commitHash) {
    content = content.replace(
      /^Status: closed$/m,
      `Status: closed\nClosed: ${new Date().toISOString()} (${opts.commitHash})`
    )
  }

  // Check all unchecked boxes
  content = content.replace(/^- \[ \]/gm, '- [x]')

  fs.writeFileSync(file, content, 'utf8')
  return { file, number: issueNumber }
}

/**
 * Reads and returns metadata from an issue file.
 *
 * @param {string} cwd - Repository root directory
 * @param {number} issueNumber
 * @returns {{ number: number, file: string, status: string, title: string, content: string } | null}
 */
export function getIssue(cwd, issueNumber) {
  const dir = getIssuesDir(cwd)
  const numStr = String(issueNumber).padStart(4, '0')
  const files = fs.readdirSync(dir).filter(f => f.startsWith(numStr + '-'))
  if (files.length === 0) return null

  const file = path.join(dir, files[0])
  const content = fs.readFileSync(file, 'utf8')

  const titleMatch = content.match(/^# Issue #\d+: (.+)$/m)
  const statusMatch = content.match(/^Status: (\w+)$/m)

  return {
    number: issueNumber,
    file,
    title: titleMatch ? titleMatch[1] : '',
    status: statusMatch ? statusMatch[1] : 'unknown',
    content,
  }
}
