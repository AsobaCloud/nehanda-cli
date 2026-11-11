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

  await test('AGENTS-PERM-3: planning stays read-only even with an already-approved plan row', async () => {
    const root = fs.mkdtempSync(path.join(os.tmpdir(), 'agents-perm-3-'))
    const dbPath = path.join(root, 'state.db')
    const db = openStore(dbPath)
    const cid = 'planning-no-mutation', sid = 'planning-session'
    const source = path.join(root, 'src', 'module.mjs')
    fs.mkdirSync(path.dirname(source), { recursive: true })
    fs.writeFileSync(source, 'export const value = 41\n')
    const originalHash = sha256(fs.readFileSync(source))
    try {
      db.prepare("INSERT INTO conversations(id, project_dir, phase) VALUES (?, ?, 'planning')").run(cid, root)
      db.prepare('INSERT INTO sessions(session_id, conversation_id) VALUES (?, ?)').run(sid, cid)
      // Deliberately seed prior approval as a boundary probe; it must never
      // grant write access while the actual phase is still planning.
      db.prepare("INSERT INTO plans(conversation_id, content, hash, status) VALUES (?, ?, 'fixture-hash', 'approved')")
        .run(cid, '## Issue Reference\n#1\n## Behavioral Specification\nGiven x\nWhen y\nThen z\n## Coverage Matrix\n| Scenario | Test File |\n| x | tests/x.mjs |')
      const ctx = { cwd: root, conversationId: cid, sessionId: sid }
      for (const [name, input] of [
        ['Write', { file_path: 'src/module.mjs', content: 'CHANGED' }],
        ['Edit', { file_path: 'src/module.mjs', old_string: '41', new_string: '42' }],
        ['NotebookEdit', { file_path: 'src/module.mjs', cell_id: '1', new_source: 'CHANGED' }],
        ['ApplyPatch', { file_path: 'src/module.mjs', patch: 'CHANGED' }],
        ['Bash', { command: 'printf CHANGED > src/module.mjs' }],
        ['Bash', { command: 'node -e "require(\'fs\').writeFileSync(\'src/module.mjs\',\'CHANGED\')"' }],
      ]) {
        const result = await executeBuiltinTool(db, ctx, name, input, {})
        rejectResult(name, result)
        assert.equal(sha256(fs.readFileSync(source)), originalHash, `${name} mutated production during planning`)
      }
    } finally {
      closeStore(dbPath)
      fs.rmSync(root, { recursive: true, force: true })
    }
  })

  await test('AGENTS-PERM-4: create_tests rejects symlink escapes but implement permits production fixes', async () => {
    const root = fs.mkdtempSync(path.join(os.tmpdir(), 'agents-perm-4-'))
    const dbPath = path.join(root, 'state.db')
    const db = openStore(dbPath)
    const cid = 'symlink-escape', sid = 'symlink-session'
    fs.mkdirSync(path.join(root, 'tests'), { recursive: true })
    fs.mkdirSync(path.join(root, 'src'), { recursive: true })
    const source = path.join(root, 'src', 'module.mjs')
    fs.writeFileSync(source, 'export const original = true\n')
    fs.symlinkSync(path.join(root, 'src'), path.join(root, 'tests', 'escape'))
    const baseline = sha256(fs.readFileSync(source))
    try {
      db.prepare("INSERT INTO conversations(id, project_dir, phase) VALUES (?, ?, 'create_tests')").run(cid, root)
      db.prepare('INSERT INTO sessions(session_id, conversation_id) VALUES (?, ?)').run(sid, cid)
      const ctx = { cwd: root, conversationId: cid, sessionId: sid }
      for (const [name, input] of [
        ['Write', { file_path: 'tests/escape/module.mjs', content: 'WRONG' }],
        ['Edit', { file_path: 'tests/escape/module.mjs', old_string: 'true', new_string: 'false' }],
      ]) {
        rejectResult(name, await executeBuiltinTool(db, ctx, name, input, {}))
        assert.equal(sha256(fs.readFileSync(source)), baseline, `${name} escaped via tests/ symlink`)
      }
      db.prepare("UPDATE conversations SET phase='implement' WHERE id=?").run(cid)
      const allowed = await executeBuiltinTool(db, ctx, 'Write', {
        file_path: 'src/module.mjs', content: 'export const original = false\n',
      }, {})
      assert.equal(allowed.is_error, false, 'real implement phase must permit approved source edits')
      assert.match(fs.readFileSync(source, 'utf8'), /false/)
    } finally {
      closeStore(dbPath)
      fs.rmSync(root, { recursive: true, force: true })
    }
  })

  await test('AGENTS-PERM-5: Edit preserves phase ownership and rejects ambiguous replacements without mutation', async () => {
    const root = fs.mkdtempSync(path.join(os.tmpdir(), 'agents-edit-'))
    const dbPath = path.join(root, 'state.db'), db = openStore(dbPath)
    const cid = 'edit-ownership', sid = 'edit-session'
    fs.mkdirSync(path.join(root, 'tests'))
    fs.mkdirSync(path.join(root, 'src'))
    const regression = path.join(root, 'tests', 'regression.mjs')
    const source = path.join(root, 'src', 'module.mjs')
    fs.writeFileSync(regression, 'RED RED')
    fs.writeFileSync(source, 'export const value = 1\n')
    try {
      db.prepare("INSERT INTO conversations(id,project_dir,phase) VALUES (?,?,'create_tests')").run(cid,root)
      db.prepare('INSERT INTO sessions(session_id,conversation_id) VALUES (?,?)').run(sid,cid)
      const ctx = {cwd:root,conversationId:cid,sessionId:sid}
      const edit = input => executeBuiltinTool(db,ctx,'Edit',input,{})
      const ambiguous = await edit({file_path:'tests/regression.mjs',old_string:'RED',new_string:'GREEN'})
      assert.equal(ambiguous.is_error,true)
      assert.match(ambiguous.content,/matches 2 locations/)
      assert.equal(fs.readFileSync(regression,'utf8'),'RED RED')
      const replaceAll = await edit({file_path:'tests/regression.mjs',old_string:'RED',new_string:'GREEN',replace_all:true})
      assert.equal(replaceAll.is_error,false)
      assert.equal(fs.readFileSync(regression,'utf8'),'GREEN GREEN')
      const absent = await edit({file_path:'tests/regression.mjs',old_string:'MISSING',new_string:'GREEN',replace_all:true})
      assert.equal(absent.is_error,true)
      assert.equal(fs.readFileSync(regression,'utf8'),'GREEN GREEN')
      const forbidden = await edit({file_path:'src/module.mjs',old_string:'1',new_string:'2'})
      assert.equal(forbidden.is_error,true)
      assert.equal(fs.readFileSync(source,'utf8'),'export const value = 1\n')
      db.prepare("UPDATE conversations SET phase='implement' WHERE id=?").run(cid)
      const allowed = await edit({file_path:'src/module.mjs',old_string:'1',new_string:'2'})
      assert.equal(allowed.is_error,false)
      assert.equal(fs.readFileSync(source,'utf8'),'export const value = 2\n')
      for (const input of [{},{file_path:'src/module.mjs'},{file_path:'src/module.mjs',old_string:'2'},
        {file_path:'../outside.mjs',old_string:'x',new_string:'y'},
        {file_path:'src/missing.mjs',old_string:'x',new_string:'y'},
        {file_path:'src/module.mjs',old_string:'MISSING',new_string:'y'}]) {
        assert.equal((await edit(input)).is_error,true)
        assert.equal(fs.readFileSync(source,'utf8'),'export const value = 2\n')
      }
    } finally {closeStore(dbPath);fs.rmSync(root,{recursive:true,force:true})}
  })

  console.log(`\n${passed} passed, ${failed} failed`)
})().catch((err) => {
  console.error(err)
  process.exit(1)
})

if (failed > 0) process.exitCode = 1
