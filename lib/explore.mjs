import fs from 'node:fs'
import path from 'node:path'

// Directories to skip entirely during exploration
const SKIP_DIRS = new Set([
  'node_modules', '.git', '.issues', 'dist', 'build', 'tmp', '.tmp',
  'coverage', '.nyc_output', '__pycache__', '.venv', 'venv',
  '.next', '.nuxt', 'out',
])

// Documentation files receive highest priority (read first)
const DOC_FILENAMES = new Set([
  'README.md', 'readme.md', 'README', 'readme',
  'CONTRIBUTING.md', 'CONTRIBUTING', 'ARCHITECTURE.md', 'ARCHITECTURE',
  'AGENTS.md', 'GEMINI.md', 'CHANGELOG.md', 'CHANGES.md',
  'DESIGN.md', 'OVERVIEW.md', 'ONBOARDING.md', 'SETUP.md',
  'MAP.md', 'RUNBOOK.md', 'OPERATIONS.md', 'SPECIFICATION.md', 'SPEC.md',
])

// Secondary documentation: anything under docs/ or .docs/
const DOC_DIRS = new Set(['docs', '.docs', 'documentation', 'doc'])

/**
 * Scans a repository directory and returns files in documentation-first order.
 *
 * Priority order:
 *   1. Root-level named documentation files (README.md, CONTRIBUTING.md, etc.)
 *   2. All Markdown files inside docs/ subdirectories
 *   3. Root-level *.md files not already listed
 *   4. Configuration files (package.json, pyproject.toml, etc.)
 *   5. All other source files (alphabetical, depth-first)
 *
 * @param {string} cwd - The repository root to scan
 * @returns {string[]} - Absolute paths in exploration priority order
 */
export function getDocPrioritizedFiles(cwd) {
  const abs = path.resolve(cwd)

  // Tier 1: High-priority named root documentation files
  const tier1 = []
  for (const name of DOC_FILENAMES) {
    const f = path.join(abs, name)
    if (fs.existsSync(f) && fs.statSync(f).isFile()) {
      tier1.push(f)
    }
  }

  // Collect all files in the repo (excluding SKIP_DIRS)
  const allFiles = []
  function walk(dir) {
    let entries
    try {
      entries = fs.readdirSync(dir, { withFileTypes: true })
    } catch {
      return
    }
    for (const entry of entries) {
      if (entry.isDirectory()) {
        if (!SKIP_DIRS.has(entry.name)) walk(path.join(dir, entry.name))
      } else if (entry.isFile()) {
        allFiles.push(path.join(dir, entry.name))
      }
    }
  }
  walk(abs)

  const tier1Set = new Set(tier1)

  // Tier 2: Markdown files inside docs/ directories
  const tier2 = allFiles.filter(f => {
    if (tier1Set.has(f)) return false
    const rel = path.relative(abs, f)
    const parts = rel.split(path.sep)
    return parts.length > 1 && DOC_DIRS.has(parts[0]) && f.endsWith('.md')
  })

  const tier2Set = new Set(tier2)

  // Tier 3: Remaining root-level *.md files
  const tier3 = allFiles.filter(f => {
    if (tier1Set.has(f) || tier2Set.has(f)) return false
    const rel = path.relative(abs, f)
    return !rel.includes(path.sep) && f.endsWith('.md')
  })

  const tier3Set = new Set(tier3)

  // Tier 4: Config files
  const CONFIG_NAMES = new Set([
    'package.json', 'package-lock.json', 'yarn.lock', 'pnpm-lock.yaml',
    'pyproject.toml', 'setup.py', 'setup.cfg', 'Cargo.toml',
    '.env.example', '.env.sample', 'tsconfig.json', 'jest.config.js',
    'jest.config.mjs', 'eslint.config.js', '.eslintrc.js', '.eslintrc.json',
  ])
  const tier4 = allFiles.filter(f => {
    if (tier1Set.has(f) || tier2Set.has(f) || tier3Set.has(f)) return false
    const base = path.basename(f)
    return CONFIG_NAMES.has(base)
  })

  const tier4Set = new Set(tier4)

  // Tier 5: Everything else
  const tier5 = allFiles.filter(f =>
    !tier1Set.has(f) && !tier2Set.has(f) && !tier3Set.has(f) && !tier4Set.has(f)
  )

  return [...tier1, ...tier2, ...tier3, ...tier4, ...tier5]
}
