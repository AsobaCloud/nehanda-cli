/**
 * #47: the bounded-table contract for JSON tool results, asserted directly on
 * the renderer so it no longer depends on Bash being runnable from `idle`.
 */
import test from 'node:test'
import assert from 'node:assert/strict'
import { formatToolResult } from '../../lib/ui.mjs'

const rows = Array.from({ length: 12 }, (_, i) => ({ asset_id: `inverter-${i}`, power_kw: 42 + i }))

test('AC-2: Given 12 JSON rows, When a tool result renders, Then the table is capped and the remainder is counted', () => {
  const output = formatToolResult('Read', JSON.stringify(rows), false)
  assert.match(output, /power_kw/, 'header must render the row keys')
  assert.match(output, /inverter-9/, 'the first 10 rows must render')
  assert.match(output, /12 rows/, 'the total row count must be reported')
  assert.match(output, /2 more rows/, 'rows beyond the cap must be summarised')
  assert.ok(!output.includes('inverter-11'), 'rows beyond the cap must not render')
})

test('AC-2: Given a non-array tool result, When it renders, Then no table is produced', () => {
  const output = formatToolResult('Bash', 'plain text output', false)
  assert.ok(!/rows —/.test(output), 'plain text must not be reported as a table')
})
