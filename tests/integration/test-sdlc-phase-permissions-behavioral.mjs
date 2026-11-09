import assert from 'node:assert'
import crypto from 'node:crypto'
import fs from 'node:fs'
import os from 'node:os'
import path from 'node:path'

import { openStore, closeStore } from '../../lib/store.mjs'
import { executeBuiltinTool } from '../../lib/tools.mjs'

let passed = 0
let failed = 0

function test(name, fn) {
  return Promise.resolve().then(fn).then(() => {
    console.log('✓ ' + name)
    passed++
  }).catch((err) => {
    console.log('✗ ' + name)
    console.log('  ' + (err && err.stack ? err.stack : err))
    failed++
  })
}

function sha256(value) {
  return crypto.createHash('sha256').update(value).digest('hex')
}

function rejectResult(toolName, result) {
  assert.equal(result?.is_error, true, `${toolName}: expected rejection, received: ${JSON.stringify(result)}`)
  assert.match(result?.content || '', /not available|unknown tool|path outside|root clutter/i, `${toolName}: unexpected rejection reason: ${JSON.stringify(result)}`)
}

await (async () => {
  await test('AGENTS-PERM-1: explore phase permits Read but rejects all mutation channels', async () => {
    const workspace = fs.mkdtempSync(path.join(os.tmpdir(), 'agents-perm-1-'))
    const dbPath = path.join(workspace, 'phase-permissions.sqlite')
    const sourcePath = path.join(workspace, 'protected-source.txt')
    const sourceContent = 'ORIGINAL-CONTENT\nSHOULD-REMAIN-BYTE-STABLE\n'
    const markerPath = path.join(workspace, 'BYPASS-CREATED')

    fs.writeFileSync(sourcePath, sourceContent, 'utf8')
    const db = openStore(dbPath)
    const conversationId = 'c-agents-perm-1'
    const sessionId = 's-agents-perm-1'
    const baselineEntries = fs.readdirSync(workspace).sort()

    try {
      db.prepare(`INSERT INTO conversations (id, project_dir, phase) VALUES (?, ?, 'explore')`).run(conversationId, workspace)
      db.prepare(`INSERT INTO sessions (session_id, conversation_id) VALUES (?, ?)`).run(sessionId, conversationId)

      const ctx = { conversationId, sessionId, cwd: workspace }
      const io = {}

      const reject = async (toolName, input) => {
        const result = await executeBuiltinTool(db, ctx, toolName, input, io)
        rejectResult(toolName, result)
        return result
      }

      await reject('Write', { file_path: sourcePath, content: 'MUTATED-CONTENT' })
      await reject('Edit', { file_path: sourcePath, old_string: 'ORIGINAL', new_string: 'MUTATED' })
      await reject('ApplyPatch', {
        file_path: sourcePath,
        path: sourcePath,
        patch: '--- a\n+++ b\n@@\n-ORIGINAL-CONTENT\n+MUTATED-CONTENT\n',
      })

      const bashResult = await executeBuiltinTool(db, ctx, 'Bash', {
        command: `printf 'MUTATION' > ${JSON.stringify(markerPath)}`,
      }, io)
      rejectResult('Bash', bashResult)

      const readResult = await executeBuiltinTool(db, ctx, 'Read', { file_path: sourcePath }, io)
      assert.notEqual(readResult?.is_error, true, `Read should remain available, received: ${JSON.stringify(readResult)}`)
      assert.match(readResult?.content || '', /ORIGINAL-CONTENT/, `Read must return original content: ${JSON.stringify(readResult)}`)

      const afterEntries = fs.readdirSync(workspace).sort()
      assert.deepEqual(afterEntries, baselineEntries, `workspace entries changed: ${JSON.stringify(afterEntries)} vs ${JSON.stringify(baselineEntries)}`)
      assert.equal(fs.readFileSync(sourcePath, 'utf8'), sourceContent, 'protected source file was mutated')
      assert.equal(fs.existsSync(markerPath), false, 'Bash bypassed explore restrictions and created a marker')
    } finally {
      closeStore(dbPath)
      fs.rmSync(workspace, { recursive: true, force: true })
    }
  })

  await test('AGENTS-PERM-2: create_tests permits authorized test mutation but blocks production mutation', async () => {
    const workspace = fs.mkdtempSync(path.join(os.tmpdir(), 'agents-perm-2-'))
    const dbPath = path.join(workspace, 'create-tests-permissions.sqlite')
    const sourcePath = path.join(workspace, 'src', 'module.mjs')
    const secondSourcePath = path.join(workspace, 'src', 'admin.mjs')
    const sourceContent = 'export const source = "BASE"\nconst secret = "SHOULD-REMAIN"\n'
    const traversedPath = 'tests/../src/module.mjs'
    const markerPath = path.join(workspace, 'UNAUTHORIZED-CREATE-TESTS')

    fs.mkdirSync(path.dirname(sourcePath), { recursive: true })
    fs.writeFileSync(sourcePath, sourceContent, 'utf8')
    fs.writeFileSync(secondSourcePath, 'export const admin = "BASE"\n', 'utf8')

    const db = openStore(dbPath)
    const conversationId = 'c-agents-perm-2'
    const sessionId = 's-agents-perm-2'
    const baselineEntries = fs.readdirSync(workspace).sort()
    const beforeSourceHash = sha256(fs.readFileSync(sourcePath))
    const beforeSecondHash = sha256(fs.readFileSync(secondSourcePath))

    try {
      db.prepare(`INSERT INTO conversations (id, project_dir, phase) VALUES (?, ?, 'create_tests')`).run(conversationId, workspace)
      db.prepare(`INSERT INTO sessions (session_id, conversation_id) VALUES (?, ?)`).run(sessionId, conversationId)

      const ctx = { conversationId, sessionId, cwd: workspace }
      const io = {}

      const reject = async (toolName, input) => {
        const result = await executeBuiltinTool(db, ctx, toolName, input, io)
        rejectResult(toolName, result)
        return result
      }

      const permittedWrite = await executeBuiltinTool(db, ctx, 'Write', {
        file_path: 'tests/regression.mjs',
        content: 'assert.equal(1, 1)\n',
      }, io)
      assert.notEqual(permittedWrite?.is_error, true, `Write to tests/regression.mjs should succeed, received: ${JSON.stringify(permittedWrite)}`)
      assert.equal(fs.readFileSync(path.join(workspace, 'tests', 'regression.mjs'), 'utf8'), 'assert.equal(1, 1)\n')

      await reject('Write', { file_path: sourcePath, content: 'MUTATED-SOURCE' })
      await reject('Edit', { file_path: sourcePath, old_string: 'BASE', new_string: 'MUTATED' })
      await reject('ApplyPatch', { file_path: sourcePath, patch: '--- a\n+++ b\n@@\n-export const source = "BASE"\n+export const source = "MUTATED"\n' })
      await reject('Write', { file_path: traversedPath, content: 'TRAVERSAL-MUTATED-SOURCE' })

      const legitBash = await executeBuiltinTool(db, ctx, 'Bash', {
        command: `node -e "console.log('LEGIT-BASH-OK')"`,
      }, io)
      assert.notEqual(legitBash?.is_error, true, `Legitimate Bash test command should succeed, received: ${JSON.stringify(legitBash)}`)
      assert.match(legitBash?.content || '', /LEGIT-BASH-OK/, `Bash did not execute legitimate command: ${JSON.stringify(legitBash)}`)

      const redirectBash = await executeBuiltinTool(db, ctx, 'Bash', {
        command: `printf 'SHELL-REDIRECT-MUTATION' > ${JSON.stringify(sourcePath)}`,
      }, io)
      rejectResult('Bash(redirect)', redirectBash)
      assert.equal(fs.existsSync(markerPath), false, 'Bash redirection created an unauthorized marker')

      const indirectBash = await executeBuiltinTool(db, ctx, 'Bash', {
        command: `node -e "require('fs').writeFileSync(${JSON.stringify(sourcePath)}, 'NODE-E-MUTATION')"` ,
      }, io)
      rejectResult('Bash(node -e)', indirectBash)

      assert.equal(sha256(fs.readFileSync(sourcePath)), beforeSourceHash, 'create_tests phase allowed production source mutation')
      assert.equal(sha256(fs.readFileSync(secondSourcePath)), beforeSecondHash, 'create_tests phase mutated an unrelated production file')
      assert.equal(fs.existsSync(path.join(workspace, 'src', 'attacker.mjs')), false, 'an unauthorized production file was created')
      assert.equal(fs.existsSync(markerPath), false, 'an unauthorized marker was created')

      const afterEntries = fs.readdirSync(workspace).sort()
      assert.ok(Array.isArray(baselineEntries), 'baseline entries must exist')
      assert.ok(Array.isArray(afterEntries), 'after entries must exist')
    } finally {
      closeStore(dbPath)
      fs.rmSync(workspace, { recursive: true, force: true })
    }
  })

  console.log(`\n${passed} passed, ${failed} failed`)
})().catch((err) => {
  console.error(err)
  process.exit(1)
})

if (failed > 0) process.exitCode = 1
