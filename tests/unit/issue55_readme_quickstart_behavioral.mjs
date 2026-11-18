import test from 'node:test'
import assert from 'node:assert/strict'
import fs from 'node:fs'
import path from 'node:path'

const README_PATH = path.resolve(process.cwd(), 'README.md')

test('Issue #55 - README 5-minute quickstart behavioral contracts', async (t) => {
  const content = fs.readFileSync(README_PATH, 'utf8')
  const lines = content.split('\n')

  await t.test('Contract 1: README must be concise starting point (< 250 lines)', () => {
    assert.ok(
      lines.length <= 250,
      `README is currently ${lines.length} lines, should be <= 250 lines to serve as an onboarding starting point`
    )
  })

  await t.test('Contract 2: README must prominently link to https://code.asoba.co/nehanda-cli for deeper dive', () => {
    assert.ok(
      content.includes('https://code.asoba.co/nehanda-cli'),
      'README must include links to https://code.asoba.co/nehanda-cli for exhaustive documentation'
    )
  })

  await t.test('Contract 3: README must include First 5 Minutes / Quickstart heading', () => {
    assert.match(
      content,
      /#+.*(5\s*Minutes|Quickstart)/i,
      'README must have a 5 Minutes to Value / Quickstart section heading'
    )
  })

  await t.test('Contract 4: README must cover Download and Installation (npm, curl, checksums)', () => {
    assert.ok(content.includes('@asobacloud/nehanda'), 'Must document npm package @asobacloud/nehanda')
    assert.ok(content.includes('https://asoba.co/releases/install.sh'), 'Must document curl installer')
    assert.ok(content.includes('https://asoba.co/releases/SHA256SUMS'), 'Must document SHA256SUMS')
  })

  await t.test('Contract 5: README must document BYOK (Bring Your Own Key) setup', () => {
    assert.match(content, /BYOK|Bring Your Own Key/i, 'Must have BYOK section')
    assert.ok(content.includes('/model'), 'Must document /model wizard')
    assert.ok(content.includes('/key'), 'Must document /key command')
  })

  await t.test('Contract 6: README must document Working the SDLC in 5 minutes', () => {
    assert.match(content, /SDLC.*5\s*minute/i, 'Must have Working the SDLC in 5 Minutes section')
    assert.ok(content.includes('idle'), 'Must mention SDLC phases')
    assert.ok(content.includes('explore'), 'Must mention explore')
    assert.ok(content.includes('diagnose'), 'Must mention diagnose')
    assert.ok(content.includes('create_tests'), 'Must mention create_tests')
    assert.ok(content.includes('planning'), 'Must mention planning')
    assert.ok(content.includes('implement'), 'Must mention implement')
    assert.ok(content.includes('verify'), 'Must mention verify')
    assert.ok(content.includes('safety'), 'Must mention safety')
    assert.ok(content.includes('confirm_fix'), 'Must mention confirm_fix')
  })
})
