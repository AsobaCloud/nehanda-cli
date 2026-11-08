import assert from 'node:assert/strict';
import { executeWorkflowStep } from '../../lib/orchestrate.mjs';

/**
 * Addendum 1 Test D3: Real Ollama beam:cloud Boundary Mandate Enforcement
 * Environment Gate: OLLAMA_E2E_TEST=1
 */
async function runRealOllamaMandateBoundaryTest() {
  if (process.env.OLLAMA_E2E_TEST !== '1') {
    console.log('Skipping test-d3-ollama-agents-md-mandate-boundary-behavioral.mjs: OLLAMA_E2E_TEST!=1');
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

  console.log('--- Running test-d3-ollama-agents-md-mandate-boundary-behavioral.mjs against beam:cloud ---');

  const context = { currentPhase: 'planning' };
  const stepResult = await executeWorkflowStep({
    modelConfig: realModelConfig,
    timeoutMs: 120000,
    context,
    requestedAction: 'transition_to_verify',
  });

  assert.equal(stepResult.success, false, 'Invalid LLM transition request must be rejected');
  assert.equal(context.currentPhase, 'planning', 'Phase must remain unchanged in planning');
  assert.ok(
    stepResult.rejectionReason.startsWith('AGENTS'),
    `Rejection reason must explicitly cite AGENTS mandate block (Actual: ${stepResult.rejectionReason})`
  );

  console.log('✔ Real Ollama beam:cloud mandate boundary rejection test passed.');
}

runRealOllamaMandateBoundaryTest().catch((err) => {
  console.error('Test Failed:', err);
  process.exit(1);
});