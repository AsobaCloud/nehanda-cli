#!/usr/bin/env node
/**
 * Behavioral tests — full /help catalog + /mcp env unset (#9)
 *
 * HP-1: formatTuiHelpText includes every /mcp and /key variant
 * HP-2: topLevelSlashCommands excludes subcommands
 * HP-3: filterSlashAutocomplete('/mcp ') includes unset / set forms
 * HP-4: parseMcpEnvParts unset/clear → action unset
 * HP-5: parseMcpEnvParts server+NAME without VALUE → usage_error (no silent clear)
 * HP-6: parseMcpEnvParts set with VALUE → action set
 * HP-7: parseMcpEnvParts list forms
 */

import assert from 'node:assert'
import {
  TUI_SLASH_COMMANDS,
  formatTuiHelpText,
  topLevelSlashCommands,
  filterSlashAutocomplete,
  parseMcpEnvParts,
} from '../../lib/slashCommands.mjs'

let passed = 0
let failed = 0

function test(name, fn) {
  try {
    fn()
    console.log(`✓ ${name}`)
    passed++
  } catch (err) {
    console.error(`✗ ${name}\n  ${err.stack || err.message}`)
    failed++
  }
}

console.log('Running help/commands behavioral tests (#9)…\n')

test('HP-1: /help text lists all mcp and key subcommands', () => {
  const help = formatTuiHelpText()
  const required = [
    '/model',
    '/key openrouter',
    '/key openai',
    '/key zhipu',
    '/key nehanda',
    '/config',
    '/mcp list',
    '/mcp status',
    '/mcp reload',
    '/mcp add',
    '/mcp env',
    '/mcp env <server> <NAME> <VALUE>',
    '/mcp env unset',
    '/clear',
    '/retry',
    '/help',
    '/exit',
  ]
  for (const frag of required) {
    assert.ok(help.includes(frag), `help missing: ${frag}`)
  }
  assert.ok(!/\/mcp env <server> <KEY>$/m.test(help.split('\n').find(l => l.includes('Clear')) || ''), 'must not document silent clear')
  assert.ok(TUI_SLASH_COMMANDS.some(c => c.name.includes('unset')), 'catalog must include unset')
})

test('HP-2: top-level autocomplete excludes subcommands', () => {
  const tops = topLevelSlashCommands().map(c => c.name)
  assert.ok(tops.includes('/mcp'))
  assert.ok(tops.includes('/key'))
  assert.ok(!tops.some(n => n.startsWith('/mcp ')))
  assert.ok(!tops.some(n => n.startsWith('/key ')))
})

test('HP-3: /mcp prefix autocomplete surfaces unset', () => {
  const items = filterSlashAutocomplete('/mcp')
  const names = items.map(c => c.name)
  assert.ok(names.some(n => n.includes('unset')), `expected unset in ${names.join(', ')}`)
  assert.ok(names.some(n => n.includes('<VALUE>')), 'expected set form with VALUE')
})

test('HP-4: unset and clear parse to unset action', () => {
  assert.deepStrictEqual(
    parseMcpEnvParts(['env', 'unset', 'jupyter', 'JUPYTER_TOKEN']),
    { action: 'unset', server: 'jupyter', name: 'JUPYTER_TOKEN' },
  )
  assert.deepStrictEqual(
    parseMcpEnvParts(['env', 'clear', 'asoba', 'ASOBA_API_KEY']),
    { action: 'unset', server: 'asoba', name: 'ASOBA_API_KEY' },
  )
})

test('HP-5: server + NAME without VALUE is usage_error (no clear)', () => {
  const r = parseMcpEnvParts(['env', 'jupyter', 'JUPYTER_TOKEN'])
  assert.strictEqual(r.action, 'usage_error')
  assert.ok(/unset/i.test(r.error), 'error should mention unset')
  assert.ok(/VALUE/i.test(r.error), 'error should mention VALUE')
})

test('HP-6: set with VALUE', () => {
  const r = parseMcpEnvParts(['env', 'jupyter', 'JUPYTER_TOKEN', 'sk-abc', 'extra'])
  assert.strictEqual(r.action, 'set')
  assert.strictEqual(r.server, 'jupyter')
  assert.strictEqual(r.name, 'JUPYTER_TOKEN')
  assert.strictEqual(r.value, 'sk-abc extra')
})

test('HP-7: list_all and list_server', () => {
  assert.deepStrictEqual(parseMcpEnvParts(['env']), { action: 'list_all' })
  assert.deepStrictEqual(parseMcpEnvParts(['env', 'jupyter']), { action: 'list_server', server: 'jupyter' })
})

test('HP-4b: incomplete unset is usage_error', () => {
  const r = parseMcpEnvParts(['env', 'unset', 'jupyter'])
  assert.strictEqual(r.action, 'usage_error')
})

console.log(`\n${passed} passed, ${failed} failed`)
if (failed) process.exit(1)
