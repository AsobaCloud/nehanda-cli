#!/usr/bin/env node

/**
 * Integration test for Declarative AuditCodeIntegrity tool.
 * Verifies HP-1 (Registration) and HP-2 (Target-Relative Execution).
 */

import fs from 'node:fs'
import path from 'node:path'
import { fileURLToPath } from 'url'
import { dirname, join } from 'path'
import assert from 'node:assert'

const __filename = fileURLToPath(import.meta.url)
const __dirname = dirname(__filename)
const projectRoot = join(__dirname, '..', '..')

// Import tools module (under test)
const toolsMjsPath = join(projectRoot, 'lib', 'tools.mjs')
const orchestrateMjsPath = join(projectRoot, 'lib', 'orchestrate.mjs')

console.log('Testing Declarative Tool System - AuditCodeIntegrity...\n')

async function runTests() {
  const { anthropicToolDefinitions, executeBuiltinTool, EXPLORE_ONLY_TOOLS } = await import(toolsMjsPath)
  const orchestrate = await import(orchestrateMjsPath)

  // -------------------------------------------------------------
  // Test 1: Verify Registration (HP-1)
  // -------------------------------------------------------------
  console.log('Test 1: Verifying tool registration in anthropicToolDefinitions...')
  const tools = anthropicToolDefinitions()
  const auditTool = tools.find(t => t.name === 'AuditCodeIntegrity')
  
  assert.ok(auditTool, 'AuditCodeIntegrity tool should be registered in anthropicToolDefinitions()')
  assert.strictEqual(auditTool.description.includes('integrity'), true, 'Tool description should match')
  
  // Verify safety checker registrations
  assert.ok(tools.find(t => t.name === 'JsSafetyChecker'), 'JsSafetyChecker should be registered')
  assert.ok(tools.find(t => t.name === 'ShellSafetyChecker'), 'ShellSafetyChecker should be registered')
  assert.ok(tools.find(t => t.name === 'PythonSafetyChecker'), 'PythonSafetyChecker should be registered')
  
  console.log('✓ Tools registered in anthropicToolDefinitions()')

  console.log('\nTest 2: Verifying phase filter presence (EXPLORE_ONLY_TOOLS)...')
  assert.ok(EXPLORE_ONLY_TOOLS.has('AuditCodeIntegrity'), 'AuditCodeIntegrity should be in EXPLORE_ONLY_TOOLS')
  assert.ok(EXPLORE_ONLY_TOOLS.has('JsSafetyChecker'), 'JsSafetyChecker should be in EXPLORE_ONLY_TOOLS')
  assert.ok(EXPLORE_ONLY_TOOLS.has('ShellSafetyChecker'), 'ShellSafetyChecker should be in EXPLORE_ONLY_TOOLS')
  assert.ok(EXPLORE_ONLY_TOOLS.has('PythonSafetyChecker'), 'PythonSafetyChecker should be in EXPLORE_ONLY_TOOLS')
  console.log('✓ Tools present in explore-only filters')

  // -------------------------------------------------------------
  // Test 2: Verify Execution Context and Pathing (HP-2)
  // -------------------------------------------------------------
  console.log('\nTest 3: Verifying target-relative execution...')
  const tempDir = join('/tmp', 'nehanda-audit-test-' + Date.now())
  const targetDir = join(tempDir, 'temp-repo')
  const reviewDir = join(targetDir, '.review')
  
  fs.mkdirSync(targetDir, { recursive: true })
  
  // Write a dummy script with an integrity warning (aws ec2 run-instances without terminate-instances)
  const dummyScript = `#!/usr/bin/env bash
# Dummy test script
aws ec2 run-instances --image-id ami-xxxx --instance-type t2.micro
`
  const dummyScriptPath = join(targetDir, 'deploy-dummy.sh')
  fs.writeFileSync(dummyScriptPath, dummyScript, 'utf8')

  try {
    // Run the tool against targetDir
    const dbMock = {
      prepare: () => ({
        get: () => ({ phase: 'explore' })
      })
    }
    const ctxMock = { conversationId: 'test-convo', cwd: targetDir }
    
    console.log(`Running AuditCodeIntegrity tool on target: ${targetDir}`)
    const result = await executeBuiltinTool(dbMock, ctxMock, 'AuditCodeIntegrity', { target: targetDir })

    assert.strictEqual(result.is_error, true, 'Audit should return is_error: true (since there is a failure in deploy-dummy.sh)')
    
    // Check that output report exists in the targetDir/.review/ folder
    assert.strictEqual(fs.existsSync(reviewDir), true, 'Review folder should exist in the target directory')
    const files = fs.readdirSync(reviewDir)
    const reportFile = files.find(f => f.endsWith('-audit-report.json'))
    assert.ok(reportFile, 'JSON report file should be created inside target/.review/')

    const reportPath = join(reviewDir, reportFile)
    const report = JSON.parse(fs.readFileSync(reportPath, 'utf8'))
    
    // Assert target path inside report matches our targetDir
    assert.strictEqual(report.target, targetDir, 'Report target should match targeted directory path')
    
    // Assert that the file in findings is path-relative to the target base directory, not nehanda-cli
    const lifecycleFindings = report.findings.lifecycle_parity
    assert.ok(lifecycleFindings.length > 0, 'Should find lifecycle parity issues in the dummy script')
    
    const finding = lifecycleFindings[0]
    assert.strictEqual(finding.file, 'deploy-dummy.sh', 'Findings file path must be relative to the target base folder')
    console.log('✓ Report successfully saved in workspace with target-relative paths')

  } finally {
    // Clean up temporary workspace
    fs.rmSync(tempDir, { recursive: true, force: true })
  }

  // -------------------------------------------------------------
  // Test 4: Verify Safety Checker executions
  // -------------------------------------------------------------
  console.log('\nTest 4: Verifying execution of new safety checkers...')
  const dbMock = {
    prepare: () => ({
      get: () => ({ phase: 'explore' })
    })
  }

  const checkerTempDir = join('/tmp', 'nehanda-safety-test-' + Date.now())
  fs.mkdirSync(checkerTempDir, { recursive: true })

  try {
    // 4.1 Shell Safety Checker with clean script
    const okShellScript = `#!/usr/bin/env bash
set -euo pipefail
echo "OK"
`
    const okShellPath = join(checkerTempDir, 'ok.sh')
    fs.writeFileSync(okShellPath, okShellScript, 'utf8')
    
    console.log('Running ShellSafetyChecker on clean script...')
    const shellResult = await executeBuiltinTool(dbMock, { cwd: checkerTempDir }, 'ShellSafetyChecker', { target: okShellPath })
    assert.strictEqual(shellResult.is_error, false, 'Clean shell script safety check should pass')
    assert.strictEqual(shellResult.content.includes('OK:'), true, 'Should output OK status')

    // 4.2 Shell Safety Checker with bad script
    const badShellScript = `#!/usr/bin/env bash
# No strict mode enabled
echo "BAD"
`
    const badShellPath = join(checkerTempDir, 'bad.sh')
    fs.writeFileSync(badShellPath, badShellScript, 'utf8')
    
    console.log('Running ShellSafetyChecker on bad script...')
    const badShellResult = await executeBuiltinTool(dbMock, { cwd: checkerTempDir }, 'ShellSafetyChecker', { target: badShellPath })
    assert.strictEqual(badShellResult.is_error, true, 'Bad shell script safety check should fail')
    assert.strictEqual(badShellResult.content.includes('Missing \'set -euo pipefail\''), true, 'Should detect missing strict mode')

    // 4.3 JS Safety Checker with clean code
    const okJsCode = `const x = 5;\nconsole.log(x);\n`
    const okJsPath = join(checkerTempDir, 'ok.js')
    fs.writeFileSync(okJsPath, okJsCode, 'utf8')
    
    console.log('Running JsSafetyChecker on clean code...')
    const jsResult = await executeBuiltinTool(dbMock, { cwd: checkerTempDir }, 'JsSafetyChecker', { target: okJsPath })
    assert.strictEqual(jsResult.is_error, false, 'Clean JS code safety check should pass')
    assert.strictEqual(jsResult.content.includes('No issues found'), true, 'Should print clean status')

    // 4.4 JS Safety Checker with bad HTML containing broken script
    const badHtml = `<html><body><script>
function foo() { return 1; }
function foo() { return 2; }
</script></body></html>
`
    const badHtmlPath = join(checkerTempDir, 'bad.html')
    fs.writeFileSync(badHtmlPath, badHtml, 'utf8')
    
    console.log('Running JsSafetyChecker on bad HTML...')
    const badJsResult = await executeBuiltinTool(dbMock, { cwd: checkerTempDir }, 'JsSafetyChecker', { target: badHtmlPath })
    assert.strictEqual(badJsResult.is_error, true, 'Bad HTML with duplicate functions should fail')
    assert.strictEqual(badJsResult.content.includes('Duplicate function definitions'), true, 'Should detect duplicate functions')

    console.log('✓ All safety checkers executed and parsed successfully!')

  } finally {
    fs.rmSync(checkerTempDir, { recursive: true, force: true })
  }

  // -------------------------------------------------------------
  // Test 5: Verify prompt injection from declarative tool configs
  // -------------------------------------------------------------
  console.log('\nTest 5: Verifying system prompt injection...')

  // We need to import the prompt builder indirectly — build the prompts via the exported PHASE_SYSTEM
  // The buildSystemPrompt function is not exported, but we can test via the orchestrate module's internals.
  // Instead, we directly test the helper functions by importing dynamicTools and checking the configs.
  const { dynamicTools } = await import(toolsMjsPath)

  // 5.1 Verify all 4 tools have mandatory_in: ["test"]
  const mandatoryTestTools = []
  for (const [name, config] of dynamicTools) {
    if (config.phases?.mandatory_in?.includes('test')) {
      mandatoryTestTools.push(name)
    }
  }
  assert.ok(mandatoryTestTools.includes('AuditCodeIntegrity'), 'AuditCodeIntegrity should be mandatory in test')
  assert.ok(mandatoryTestTools.includes('JsSafetyChecker'), 'JsSafetyChecker should be mandatory in test')
  assert.ok(mandatoryTestTools.includes('ShellSafetyChecker'), 'ShellSafetyChecker should be mandatory in test')
  assert.ok(mandatoryTestTools.includes('PythonSafetyChecker'), 'PythonSafetyChecker should be mandatory in test')
  console.log('✓ All 4 tools marked mandatory_in test phase')

  // 5.2 Verify all 4 tools have prompt.mandatory_instruction
  for (const name of mandatoryTestTools) {
    const config = dynamicTools.get(name)
    assert.ok(config.prompt?.mandatory_instruction, `${name} should have prompt.mandatory_instruction`)
  }
  console.log('✓ All mandatory tools have prompt.mandatory_instruction text')

  // 5.3 Verify all 4 tools have prompt.available_hint
  for (const [name, config] of dynamicTools) {
    assert.ok(config.prompt?.available_hint, `${name} should have prompt.available_hint`)
  }
  console.log('✓ All tools have prompt.available_hint text')

  console.log('\n✓ All integration tests passed successfully!')
}

runTests().catch(err => {
  console.error('\n... Test failed (expected in red phase):', err)
  process.exit(1)
})
