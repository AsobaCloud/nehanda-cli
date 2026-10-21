#!/usr/bin/env node
/**
 * Unit Tests: MCP Help and Discoverability
 * Validates SCN-MCP-HELP-1, SCN-MCP-HELP-2, SCN-MCP-HELP-3
 */

import assert from 'node:assert'
import { formatHelp, formatMcpHelp } from '../../lib/ui.mjs'

let testsPassed = 0
let testsFailed = 0

function test(name, fn) {
  try {
    fn()
    console.log(`✓ ${name}`)
    testsPassed++
  } catch (error) {
    console.error(`✗ ${name}`)
    console.error(`  ${error.message}`)
    testsFailed++
  }
}

console.log('Running unit tests for MCP Help and Discoverability...\n')

// SCN-MCP-HELP-2: formatHelp includes /mcp list
test('formatHelp includes /mcp and /mcp list with tool discovery description', () => {
  const help = formatHelp('anthropic', '/tmp/test.db')
  assert.ok(help.includes('/mcp list'), 'Expected formatHelp to include /mcp list')
  assert.ok(help.includes('tool') || help.includes('Tool'), 'Expected /mcp list to mention tools')
})

// SCN-MCP-HELP-1: formatMcpHelp returns structured guide
test('formatMcpHelp returns structured guide with all subcommands', () => {
  assert.strictEqual(typeof formatMcpHelp, 'function', 'formatMcpHelp should be exported from lib/ui.mjs')
  const mcpHelp = formatMcpHelp()
  assert.ok(mcpHelp.includes('list'), 'Expected formatMcpHelp to include list')
  assert.ok(mcpHelp.includes('status'), 'Expected formatMcpHelp to include status')
  assert.ok(mcpHelp.includes('reload'), 'Expected formatMcpHelp to include reload')
  assert.ok(mcpHelp.includes('add'), 'Expected formatMcpHelp to include add')
  assert.ok(mcpHelp.includes('env'), 'Expected formatMcpHelp to include env')
  assert.ok(mcpHelp.includes('/mcp list'), 'Expected formatMcpHelp to include /mcp list usage')
})

// Summary
console.log(`\n${'='.repeat(50)}`)
console.log(`Tests passed: ${testsPassed}`)
console.log(`Tests failed: ${testsFailed}`)
console.log(`${'='.repeat(50)}`)

if (testsFailed > 0) {
  console.error('\n✗ Some tests failed')
  process.exit(1)
}

console.log('\n✓ All unit tests passed!')
