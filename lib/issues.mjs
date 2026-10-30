import fs from 'node:fs'
import path from 'node:path'

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
 * Creates a consecutively numbered structured markdown issue in /.issues/.
 *
 * @param {string} cwd - Repository root directory
 * @param {object} opts
 * @param {string} opts.title - Short issue title
 * @param {string} opts.observed - Observed (broken) behavior
 * @param {string} opts.expected - Expected (desired) behavior
 * @param {string} opts.diagnosis - Evidence-backed root cause
 * @param {string[]} opts.acceptanceCriteria - Array of AC strings (e.g. 'AC-1: ...', 'AC-2: ...')
 * @returns {{ number: number, file: string, slug: string }}
 */
export function createIssue(cwd, { title, observed, expected, diagnosis, acceptanceCriteria = [] }) {
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
Created: ${created}

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
  return { number: num, file, slug }
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
