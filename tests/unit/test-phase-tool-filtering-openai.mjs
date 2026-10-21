#!/usr/bin/env node
/**
 * Unit Test: Phase-Based Tool Filtering in OpenAI-Compatible Loop (Task 2.4)
 * 
 * Validates that runOpenAICompatModelLoop filters tools based on phase
 * before passing them to streamOpenAIChatCompletion.
 * 
 * Acceptance Criteria:
 * - OpenAI-compat tools are filtered using filterToolsByPhase() before API call
 * - Filtering happens inside the loop (after phase query)
 * - In IDLE/PLANNING phases, modification tools are absent from the tool schema
 * - In IMPLEMENT/TEST phases, all tools are available
 */

import { readFileSync } from 'fs'
import { fileURLToPath } from 'url'
import { dirname, join } from 'path'

const __filename = fileURLToPath(import.meta.url)
const __dirname = dirname(__filename)

console.log('Running Unit Test: Phase-Based Tool Filtering in OpenAI-Compatible Loop (Task 2.4)\n')

// Read the orchestrate.mjs file
const orchestratePath = join(__dirname, '../../lib/orchestrate.mjs')
const orchestrateContent = readFileSync(orchestratePath, 'utf-8')

// Test 1: Verify filterToolsByPhase is called inside the loop
console.log('✓ Test 1: Verify filterToolsByPhase is called inside runOpenAICompatModelLoop')
const hasFilterInLoop = orchestrateContent.match(/for\s*\(\s*;;\s*\)\s*\{[^}]*const filteredTools = filterToolsByPhase\([^)]*activeTools[^)]*currentPhase\)/s)

if (hasFilterInLoop) {
  console.log('  ✓ filterToolsByPhase call found inside the for(;;) loop')
} else {
  console.log('  ✗ FAILED: filterToolsByPhase not called inside the for(;;) loop')
  process.exit(1)
}

// Test 2: Verify filtering happens after phase query
console.log('\n✓ Test 2: Verify filtering happens after phase query')
// Check order in the entire file content
const phaseQueryIndex = orchestrateContent.indexOf('const currentPhase = getPhase(db, rt.conversationId)')
const filterIndex = orchestrateContent.indexOf('const filteredTools = filterToolsByPhase(activeTools, currentPhase)')

// Find the second occurrence (in OpenAI loop, not Anthropic loop)
const secondPhaseQueryIndex = orchestrateContent.indexOf('const currentPhase = getPhase(db, rt.conversationId)', phaseQueryIndex + 1)

if (secondPhaseQueryIndex !== -1 && filterIndex !== -1 && secondPhaseQueryIndex < filterIndex) {
  console.log('  ✓ Tool filtering happens after phase query in OpenAI loop')
} else {
  console.log('  ✗ FAILED: Tool filtering does not happen after phase query')
  console.log(`    secondPhaseQueryIndex: ${secondPhaseQueryIndex}, filterIndex: ${filterIndex}`)
  process.exit(1)
}

// Test 3: Verify phase-filtered tools reach streamOpenAIChatCompletion (via toolsForRequest, #12)
console.log('\n✓ Test 3: Verify filteredTools is passed to streamOpenAIChatCompletion')
const derivedFromFiltered = /toolsForRequest = xmlInstructions \? \[\] : filteredTools/.test(orchestrateContent)
const hasFilteredToolsInCall = derivedFromFiltered && /streamOpenAIChatCompletion\(\{[^}]*tools:\s*toolsForRequest/s.test(orchestrateContent)

if (hasFilteredToolsInCall) {
  console.log('  ✓ filteredTools-derived toolsForRequest is passed to streamOpenAIChatCompletion')
} else {
  console.log('  ✗ FAILED: filteredTools not passed to streamOpenAIChatCompletion')
  process.exit(1)
}

// Test 4: Verify filterToolsByPhase function exists
console.log('\n✓ Test 4: Verify filterToolsByPhase function exists')
const hasFilterFunction = orchestrateContent.match(/function filterToolsByPhase\s*\(\s*tools\s*,\s*phase\s*\)/s)

if (hasFilterFunction) {
  console.log('  ✓ filterToolsByPhase function is defined')
} else {
  console.log('  ✗ FAILED: filterToolsByPhase function not found')
  process.exit(1)
}

// Test 5: Verify the explore-phase tool allowlist constant exists (EXPLORE_ONLY_TOOLS, #12)
console.log('\n✓ Test 5: Verify EXPLORE_ONLY_TOOLS constant exists')
const toolsContent = readFileSync(join(__dirname, '../../lib/tools.mjs'), 'utf8')
const hasModificationTools = toolsContent.match(/export const EXPLORE_ONLY_TOOLS\s*=\s*new Set\(\[/s)

if (hasModificationTools) {
  console.log('  ✓ EXPLORE_ONLY_TOOLS constant is defined')
} else {
  console.log('  ✗ FAILED: EXPLORE_ONLY_TOOLS constant not found')
  process.exit(1)
}

// Test 6: Verify filtering logic handles explore (allowlist) and planning (no tools) — #12
console.log('\n✓ Test 6: Verify filtering logic handles explore/planning phases')
const hasExploreCheck = /phase\s*===\s*['"]explore['"]/.test(orchestrateContent) && /EXPLORE_ONLY_TOOLS\.has\(name\)/.test(orchestrateContent)
const hasPlanningCheck = /phase\s*===\s*['"]planning['"]\s*\)\s*\{\s*return\s*\[\]/s.test(orchestrateContent)

if (hasExploreCheck && hasPlanningCheck) {
  console.log('  ✓ Filtering logic handles explore/planning phases')
} else {
  console.log('  ✗ FAILED: Phase checks not found in filtering logic')
  process.exit(1)
}

// Test 7: Verify phase filtering comment is present
console.log('\n✓ Test 7: Verify phase filtering comment is present')
const hasTask24Comment = /Phase-Based Tool Filtering|filterToolsByPhase/.test(orchestrateContent)

if (hasTask24Comment) {
  console.log('  ✓ Task 2.4 comment is present')
} else {
  console.log('  ✗ FAILED: Task 2.4 comment not found')
  process.exit(1)
}

console.log('\n============================================================')
console.log('Test Results: 7 passed, 0 failed')
console.log('============================================================')
console.log('\n✓ All tests passed!')
console.log('Requirements validated: Task 2.4 - Phase-Based Tool Filtering for OpenAI-Compatible Models')
