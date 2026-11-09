import { withTransaction } from './store.mjs'
import childProcess from 'node:child_process'
import crypto from 'node:crypto'
import fs from 'node:fs'
import path from 'node:path'

function parseLcov(text) {
  const records = String(text || '').split(/^end_of_record$/m)
    .map((chunk) => chunk.trim())
    .filter(Boolean)

  return records.map((chunk) => {
    const lines = chunk.split(/\r?\n/)
    const fileLine = lines.find((line) => line.startsWith('SF:'))
    const brfLine = lines.find((line) => line.startsWith('BRF:'))
    const brhLine = lines.find((line) => line.startsWith('BRH:'))
    return {
      filePath: fileLine ? fileLine.slice(3).trim() : null,
      brf: Number(brfLine?.slice(4).trim()),
      brh: Number(brhLine?.slice(4).trim()),
    }
  })
}

function sha256File(filePath) {
  return crypto.createHash('sha256').update(fs.readFileSync(filePath)).digest('hex')
}

function isProductionJavaScript(filePath) {
  const normalized = filePath.replace(/\\/g, '/')
  if (!/\.(js|cjs|mjs)$/i.test(normalized)) return false
  if (
    /\/tests?\//i.test(normalized) ||
    /\.test\.[cm]?js$/i.test(normalized) ||
    /\.spec\.[cm]?js$/i.test(normalized) ||
    /\/node_modules\//i.test(normalized) ||
    /\/coverage(?:-lcov)?\//i.test(normalized) ||
    /\/dist(?:_macosx|_linux)?\//i.test(normalized)
  ) {
    return false
  }
  return true
}

function findLcovCandidates(root) {
  const results = []
  const walk = (dir, depth = 0) => {
    if (depth > 7) return
    let entries = []
    try { entries = fs.readdirSync(dir, { withFileTypes: true }) } catch { return }
    for (const entry of entries) {
      const full = path.join(dir, entry.name)
      if (entry.isDirectory()) {
        if (entry.name === 'node_modules' || entry.name === '.git') continue
        walk(full, depth + 1)
        continue
      }
      const lower = entry.name.toLowerCase()
      if (lower.endsWith('.lcov') || lower.endsWith('.coverage')) {
        results.push(full)
        continue
      }
      try {
        const maybe = fs.readFileSync(full, 'utf8').slice(0, 512)
        if (maybe.includes('TN:') && maybe.includes('BRF:')) results.push(full)
      } catch {}
    }
  }
  walk(root, 0)
  return results
}

function changedProductionPaths(root) {
  try {
    const output = childProcess.execFileSync('git', ['-C', root, 'diff', '--name-only', '--relative', 'HEAD'], {
      encoding: 'utf8',
      stdio: ['ignore', 'pipe', 'ignore'],
    }).trim()
    if (!output) return null
    return new Set((output.split(/\r?\n/)))
  } catch {
    return null
  }
}

function changedPathMatches(changedSet, root, absoluteFile) {
  if (!changedSet) return true
  const relative = path.relative(root, absoluteFile).replace(/\\/g, '/')
  for (const item of changedSet) {
    const clean = String(item || '').trim().replace(/\\/g, '/')
    if (clean === relative) return true
    if (absoluteFile === path.resolve(root, clean)) return true
  }
  return false
}

function latestPassingTest(db, conversationId) {
  const rows = db.prepare(
    `SELECT detail FROM events WHERE conversation_id = ? AND event_type = 'test_result' ORDER BY id`
  ).all(conversationId)
  let latest = null
  for (const row of rows) {
    try {
      const detail = JSON.parse(row.detail || '{}')
      if (detail.passed === true) latest = detail
    } catch {}
  }
  return latest
}

export function collectBranchCoverage(db, rt, options = {}) {
  const root = path.resolve(rt?.cwd || process.cwd())
  const conversationId = rt?.conversationId
  if (!db || !conversationId) {
    return { accepted: false, reason: 'branch coverage requires conversation and database context' }
  }

  const candidatePaths = []
  if (options.lcovPath) candidatePaths.push(path.resolve(options.lcovPath))
  else candidatePaths.push(...findLcovCandidates(root))

  const validCandidates = candidatePaths
    .map((p) => {
      try {
        const stat = fs.statSync(p)
        return { path: p, mtime: stat.mtimeMs, text: fs.readFileSync(p, 'utf8') }
      } catch {
        return null
      }
    })
    .filter(Boolean)
    .sort((a, b) => b.mtime - a.mtime)

  if (validCandidates.length === 0) {
    return { accepted: false, reason: 'branch coverage requires native Node LCOV output' }
  }

  const passingTest = latestPassingTest(db, conversationId)
  if (!passingTest) {
    return { accepted: false, reason: 'branch coverage requires an actual passing GREEN test' }
  }

  const changedSet = changedProductionPaths(root)
  let selectedCandidate = null
  let selectedRecords = []
  let productionFiles = []

  for (const candidate of validCandidates) {
    const parsed = parseLcov(candidate.text)
    const files = []
    let hasProduction = false

    for (const record of parsed) {
      if (!record.filePath || !Number.isFinite(record.brf) || !Number.isFinite(record.brh)) {
        continue
      }
      const absolute = path.resolve(root, record.filePath)
      if (!isProductionJavaScript(absolute)) continue
      if (!changedPathMatches(changedSet, root, absolute)) continue
      if (!fs.existsSync(absolute)) continue
      if (absolute === path.resolve(root, record.filePath) && record.brf <= 0) continue
      files.push({
        path: absolute,
        relativePath: path.relative(root, absolute).replace(/\\/g, '/'),
        hash: sha256File(absolute),
        brf: record.brf,
        brh: record.brh,
        ratio: record.brh / record.brf,
      })
      hasProduction = true
    }

    if (hasProduction) {
      selectedCandidate = candidate
      selectedRecords = parsed
      productionFiles = files
      break
    }
  }

  if (!selectedCandidate || productionFiles.length === 0) {
    return { accepted: false, reason: 'branch coverage output contains no changed production JavaScript files' }
  }

  const numerator = productionFiles.reduce((sum, file) => sum + file.brh, 0)
  const denominator = productionFiles.reduce((sum, file) => sum + file.brf, 0)
  if (denominator <= 0) {
    return { accepted: false, reason: 'branch coverage denominator is zero' }
  }
  const ratio = numerator / denominator

  const testFileHash = (selectedRecords
    .map((record) => record.filePath)
    .filter(Boolean)
    .find((filePath) => /(^|\/)tests?\//.test(filePath.replace(/\\/g, '/'))))
  const testPath = testFileHash
    ? path.resolve(root, testFileHash)
    : null
  const testHash = testPath && fs.existsSync(testPath) ? sha256File(testPath) : null

  const measurement = {
    metric: 'changedProductionJavaScriptBranchCoverage',
    metricType: 'branch_coverage',
    numerator,
    denominator,
    ratio,
    threshold: 70,
    files: productionFiles.map(({ hash, brf, brh, relativePath, ratio: fileRatio }) => ({
      path: relativePath,
      sha256: hash,
      brf,
      brh,
      ratio: fileRatio,
    })),
    test: {
      command: passingTest.command || passingTest.commandText || null,
      path: testPath ? path.relative(root, testPath).replace(/\\/g, '/') : (passingTest.path || null),
      sha256: testHash || passingTest.testSha256 || null,
    },
    execution: {
      executionId: crypto.randomUUID(),
      source: 'node-lcov',
      testResultExecutionId: passingTest.executionId || passingTest.execution_id || null,
      timestamp: new Date().toISOString(),
      lcovPath: path.relative(root, selectedCandidate.path).replace(/\\/g, '/'),
      lcovSha256: sha256File(selectedCandidate.path),
      allProductionRecords: selectedRecords.filter((r) => r.filePath && isProductionJavaScript(path.resolve(root, r.filePath))).map((r) => r.filePath),
    },
    accepted: true,
  }

  const fingerprint = crypto.createHash('sha256').update(JSON.stringify({
    numerator: measurement.numerator,
    denominator: measurement.denominator,
    ratio: measurement.ratio,
    files: measurement.files,
    test: measurement.test,
    lcovSha256: measurement.execution.lcovSha256,
    testExecutionId: measurement.execution.testResultExecutionId,
  })).digest('hex')

  const existingRows = db.prepare(
    `SELECT detail FROM events WHERE conversation_id = ? AND event_type = 'branch_coverage' ORDER BY id`
  ).all(conversationId)
  const duplicate = existingRows.some((row) => {
    try {
      return JSON.parse(row.detail || '{}').fingerprint === fingerprint
    } catch {
      return false
    }
  })
  if (duplicate) {
    return { accepted: true, duplicate: true, ratio, measurement, fingerprint }
  }

  withTransaction(db, () => {
    db.prepare(`INSERT INTO events (conversation_id, event_type, detail) VALUES (?, 'branch_coverage', ?)`).run(
      conversationId,
      JSON.stringify({ ...measurement, fingerprint })
    )
  })

  return { accepted: true, duplicate: false, ratio, measurement, fingerprint }
}
