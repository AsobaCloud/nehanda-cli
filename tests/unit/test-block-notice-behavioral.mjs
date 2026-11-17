/** #49: the block notice must say what the tool was trying to do, in plain language. */
import test from 'node:test'
import assert from 'node:assert/strict'
import { fixture } from '../fixtures/sdlcAcceptance.mjs'
import { createTestsGate } from '../../lib/orchestrate.mjs'
import { executeBuiltinTool } from '../../lib/tools.mjs'
import * as ui from '../../lib/ui.mjs'

const DEVICE_BLOCK = {
  ok: false,
  status: 'blocked',
  reason: 'format writes a filesystem to /dev/sda (destructive device write)',
  risk_score: 'high',
  proposed_remediation: ['review_command', 'sandbox'],
  command: 'format /dev/sda',
}

test('BN-1: Given a blocked command, When the notice renders, Then it names the command and no machine tokens', () => {
  const notice = ui.formatBlockNotice(DEVICE_BLOCK)
  assert.match(notice, /format \/dev\/sda/, `the attempted command must be shown: ${notice}`)
  assert.match(notice, /Why/i)
  assert.match(notice, /Effect/i)
  assert.match(notice, /Next/i)
  assert.doesNotMatch(notice, /review_command|proposed_remediation|risk_score/)
})

test('BN-2: Given a blocked command, When the notice renders, Then it states nothing ran and nothing changed', () => {
  const notice = ui.formatBlockNotice(DEVICE_BLOCK)
  assert.match(notice, /not (?:run|executed)/i, `must state the command did not run: ${notice}`)
  assert.match(notice, /(?:nothing|no files?|disk).{0,40}(?:changed|modified|written|touched)/i, `must state the system is unchanged: ${notice}`)
})

test('BN-3: Given a blocked Bash call, When the menu opens, Then it receives the command and cancel says nothing ran', async () => {
  const f = fixture('bn3', 'create_tests')
  let seen = null
  const io = {
    println() {}, write() {},
    async confirmBlock(payload) { seen = payload; return 'cancel' },
  }
  try {
    const result = await executeBuiltinTool(f.db, f.rt, 'Bash', { command: 'format /dev/sda' }, io)
    assert.equal(result.is_error, true)
    assert.ok(seen, 'the confirmation menu must be raised')
    assert.equal(seen.command, 'format /dev/sda', `payload must carry the command: ${JSON.stringify(seen)}`)
    assert.match(String(result.content), /format \/dev\/sda/, `refusal must name the command: ${result.content}`)
    assert.match(String(result.content), /not (?:run|executed)/i, `refusal must say nothing ran: ${result.content}`)
  } finally { f.close() }
})

test('BN-4: Given no test execution, When the create_tests gate refuses, Then the message names the cause and the recovery', async () => {
  const f = fixture('bn4', 'create_tests')
  const lines = []
  const io = { println(line) { lines.push(String(line)) } }
  try {
    const advanced = await createTestsGate(f.db, f.rt, io)
    const text = lines.join('\n')
    assert.equal(advanced, false)
    assert.doesNotMatch(text, /✓/, `a refusal must not be reported with a success glyph: ${text}`)
    assert.match(text, /Cannot advance/, `must say the phase cannot advance: ${text}`)
    assert.match(text, /no RED test execution was recorded/i, `must name the missing evidence: ${text}`)
    assert.match(text, /paused/i, `must say the phase is paused, not lost: ${text}`)
  } finally { f.close() }
})

test('BN-5: Given a headless block, When the tool refuses, Then the refusal is self-describing', async () => {
  const f = fixture('bn5', 'create_tests')
  try {
    const result = await executeBuiltinTool(f.db, f.rt, 'Bash', { command: 'sudo rm -rf /' }, null)
    const text = String(result.content)
    assert.match(text, /sudo rm -rf \//, `refusal must name the command: ${text}`)
    assert.match(text, /not (?:run|executed)/i, `refusal must say nothing ran: ${text}`)
    assert.doesNotMatch(text, /review_command/)
  } finally { f.close() }
})
