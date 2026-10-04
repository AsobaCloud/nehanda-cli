#!/usr/bin/env node
/**
 * postinstall.mjs — runs automatically after `npm install`.
 *
 * Provisions the Laya System-1 venv (~808 MB one-time download) so the CLI
 * starts instantly on every subsequent run. Skips if already provisioned.
 *
 * Exits non-zero on failure so `npm install` surfaces the error clearly.
 */

import { spawnSync } from 'node:child_process'
import fs from 'node:fs'
import path from 'node:path'
import { fileURLToPath } from 'node:url'

const __dirname = path.dirname(fileURLToPath(import.meta.url))

const NEHANDA_HOME = process.env.NEHANDA_HOME
  || path.join(process.env.HOME || '/tmp', '.nehanda')
const MARKER = path.join(NEHANDA_HOME, '.laya-ready')
const VENV_PYTHON = path.join(NEHANDA_HOME, 'venv', 'bin', 'python')
const ENSURE_SCRIPT = path.resolve(__dirname, '..', 'lib', 'scripts', 'ensure-laya-env.sh')

// Already provisioned — nothing to do.
if (fs.existsSync(MARKER) && fs.existsSync(VENV_PYTHON) && !process.env.LAYA_FORCE_REPROVISION) {
  process.stderr.write('[nehanda] Laya already provisioned, skipping.\n')
  process.exit(0)
}

if (!fs.existsSync(ENSURE_SCRIPT)) {
  process.stderr.write(
    `[nehanda] ERROR: provisioning script not found at ${ENSURE_SCRIPT}\n` +
    `  The package may be corrupted. Try: npm install @asobacloud/nehanda --force\n`
  )
  process.exit(1)
}

process.stderr.write(
  '\n[nehanda] Provisioning Laya System-1 venv (~808 MB one-time download).\n' +
  '[nehanda] This only happens once. The CLI will start instantly after this.\n\n'
)

const result = spawnSync('bash', [ENSURE_SCRIPT], {
  stdio: 'inherit',
  env: { ...process.env },
})

if (result.error) {
  process.stderr.write(`[nehanda] Failed to run provisioning script: ${result.error.message}\n`)
  process.exit(1)
}

if (result.status !== 0) {
  process.stderr.write(
    `\n[nehanda] Laya provisioning failed (exit code ${result.status}).\n` +
    `  Check the output above for details.\n` +
    `  You can retry manually: bash ${ENSURE_SCRIPT}\n`
  )
  process.exit(result.status ?? 1)
}

process.stderr.write('\n[nehanda] Laya provisioned successfully. Run: nehanda\n\n')
