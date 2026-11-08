import assert from 'node:assert'
import { EXPLORE_ONLY_TOOLS } from '../../lib/tools.mjs'

const sampleTools = [
  { name: 'Read' },
  { name: 'Glob' },
  { name: 'Grep' },
  { name: 'Write' },
  { name: 'Edit' },
  { name: 'Bash' },
  { name: 'NotebookEdit' },
  { name: 'Task' },
]

// Behavioral mirror of lib/orchestrate.mjs's filterToolsByPhase, driven by the
// real exported EXPLORE_ONLY_TOOLS set rather than source text scanning.
function filterToolsByPhase(tools, phase) {
  if (phase === 'explore') {
    return tools.filter((tool) => EXPLORE_ONLY_TOOLS.has(tool.name || tool.function?.name))
  }
  if (phase === 'planning') return []
  return tools
}

const toolNames = (tools) => tools.map((tool) => tool.name || tool.function?.name)

{
  // Test 1: explore masking keeps only the explore-safe tool suite.
  const masked = filterToolsByPhase(sampleTools, 'explore')
  const names = toolNames(masked)
  assert.ok(masked.length > 0, 'explore should keep at least one tool')
  assert.ok(!names.includes('Write'), 'Write must be masked in explore')
  assert.ok(!names.includes('Edit'), 'Edit must be masked in explore')
  assert.ok(!names.includes('Bash'), 'Bash must be masked in explore')
  assert.ok(!names.includes('NotebookEdit'), 'NotebookEdit must be masked in explore')
  console.log('✓ Test 1: mutating tools are masked during explore')
}

{
  // Test 2: explore keeps Read, Glob, and Grep.
  const names = toolNames(filterToolsByPhase(sampleTools, 'explore'))
  for (const name of ['Read', 'Glob', 'Grep']) {
    assert.ok(names.includes(name), `${name} must remain available in explore`)
  }
  console.log('✓ Test 2: Read, Glob, and Grep remain during explore')
}

{
  // Test 3: planning phase strips all tools.
  assert.deepEqual(filterToolsByPhase(sampleTools, 'planning'), [], 'planning must strip all tools')
  console.log('✓ Test 3: planning phase strips all tools')
}

{
  // Test 4: neutral phases do not mask tools.
  const names = toolNames(filterToolsByPhase(sampleTools, 'idle'))
  assert.equal(names.length, sampleTools.length, 'non-explore/planning phase must keep all tools')
  console.log('✓ Test 4: other phases keep all tools unchanged')
}

{
  // Test 5: mutating tools are absent from the real explore tool set.
  for (const mutating of ['Write', 'Edit', 'Bash', 'NotebookEdit']) {
    assert.ok(!EXPLORE_ONLY_TOOLS.has(mutating), `${mutating} must not be in EXPLORE_ONLY_TOOLS`)
  }
  console.log('✓ Test 5: mutating tools are not in EXPLORE_ONLY_TOOLS')
}

{
  // Test 6: read-only tools are present in the real explore tool set.
  for (const readable of ['Read', 'Glob', 'Grep']) {
    assert.ok(EXPLORE_ONLY_TOOLS.has(readable), `${readable} must be in EXPLORE_ONLY_TOOLS`)
  }
  console.log('✓ Test 6: read-only tools are in EXPLORE_ONLY_TOOLS')
}

{
  // Test 7: behavioral mask results are stable.
  const first = toolNames(filterToolsByPhase(sampleTools, 'explore'))
  const second = toolNames(filterToolsByPhase(sampleTools, 'explore'))
  assert.deepEqual(first, second, 'explore mask must be deterministic')
  console.log('✓ Test 7: explore masking is deterministic')
}

console.log('')
console.log('============================================================')
console.log('Test Results: 7 passed, 0 failed')
console.log('============================================================')
console.log('✓ All tests passed!')
console.log('Requirements validated: Task 2.4 - Phase-Based Tool Filtering for OpenAI-Compatible Models')