import assert from 'node:assert/strict';
import { executeSdlcWorkflow } from '../../lib/orchestrate.mjs';

/**
 * Addendum 1 Test D2: Real Ollama beam:cloud E2E SDLC Workflow Test
 * Environment Gate: OLLAMA_E2E_TEST=1
 */
async function runRealOllamaSdlcTest() {
  if (process.env.OLLAMA_E2E_TEST !== '1') {
    console.log('Skipping test-d2-ollama-issue35-observable-behavioral.mjs: OLLAMA_E2E_TEST!=1');
    return;
  }

  const realModelConfig = {
    provider: 'ollama',
    custom_model_name: 'beam:cloud',
    base_url: process.env.OLLAMA_BASE_URL || 'http://localhost:11434/v1',
    api_key: 'ollama',
  };

  try {
    const res = await fetch(`${realModelConfig.base_url}/models`, { signal: AbortSignal.timeout(3000) });
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
  } catch (err) {
    console.log(`Skipping Ollama test: endpoint unavailable at ${realModelConfig.base_url} (${err.message})`);
    return;
  }

  console.log('--- Running test-d2-ollama-issue35-observable-behavioral.mjs against beam:cloud ---');

  let hitlPromptTriggered = false;
  let operatorConfirmed = false;

  const mockOperatorInterface = {
    promptPlanApproval: async (planText) => {
      hitlPromptTriggered = true;
      assert.ok(planText.includes('## Issue Reference'), 'Plan must contain ## Issue Reference');
      assert.ok(planText.includes('## Behavioral Specification'), 'Plan must contain ## Behavioral Specification');
      assert.ok(planText.includes('## Coverage Matrix'), 'Plan must contain ## Coverage Matrix');
      return 'y';
    },
    confirmCompletion: async () => {
      operatorConfirmed = true;
      return 'Y';
    },
  };

  const workflowResult = await executeSdlcWorkflow({
    modelConfig: realModelConfig,
    timeoutMs: 120000,
    operator: mockOperatorInterface,
    issueRef: 'Issue #35',
  });

  assert.ok(hitlPromptTriggered, 'HITL approval prompt [y/N] must be triggered during planning');
  assert.ok(workflowResult.redTestEvidence, 'Real RED test evidence must be recorded');
  assert.notEqual(workflowResult.redTestEvidence.exit_code, 0, 'RED test exit code must be non-zero');

  assert.equal(workflowResult.safetyResults.length, 3, 'All 3 safety checkers must be executed');
  for (const s of workflowResult.safetyResults) {
    assert.ok(s.command, 'Safety result must include executed command');
    assert.equal(s.exit_code, 0, 'Safety checker must return exit code 0');
  }

  assert.ok(operatorConfirmed, 'Operator explicit Y confirmation required before task completion');
  assert.equal(workflowResult.finalPhase, 'done', 'Workflow must complete in done phase');

  console.log('✔ Real Ollama beam:cloud E2E SDLC workflow test passed.');
}

runRealOllamaSdlcTest().catch((err) => {
  console.error('Test Failed:', err);
  process.exit(1);
});