import assert from 'node:assert/strict';
import { runExplorationLoop } from '../../lib/orchestrate.mjs';

/**
 * Addendum 1 Test D1: Real Ollama beam:cloud Bounded Exploration Test
 * Environment Gate: OLLAMA_E2E_TEST=1
 */
async function runRealOllamaExplorationTest() {
  if (process.env.OLLAMA_E2E_TEST !== '1') {
    console.log('Skipping test-d1-ollama-exploration-bounded-behavioral.mjs: OLLAMA_E2E_TEST!=1');
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
    return; // Exit 0 cleanly without false pass
  }

  console.log('--- Running test-d1-ollama-exploration-bounded-behavioral.mjs against beam:cloud ---');

  const fixtureWorkspace = {
    canonicalPaths: ['docs/MAP.md', 'lib/workflow.mjs'],
    readLogs: [],
  };

  const result = await runExplorationLoop({
    modelConfig: realModelConfig,
    timeoutMs: 120000,
    workspace: fixtureWorkspace,
  });

  // Assertions (HP-1, HP-2)
  assert.equal(result.canonicalReads.get('docs/MAP.md'), 1, 'Canonical file docs/MAP.md must be read exactly once');
  assert.ok(result.logs.some(l => l.includes('already read...')), 'Duplicate canonical read attempt must be logged as already read');
  assert.ok(result.requestCount <= 4, `Provider request count must be <= 4 (Actual: ${result.requestCount})`);
  assert.ok(result.logs.some(l => l.includes('Exploration complete.')), 'Must log "Exploration complete."');
  assert.equal(result.finalPhase, 'planning', 'Must cleanly handoff to planning phase');

  console.log('✔ Real Ollama beam:cloud exploration test passed.');
}

runRealOllamaExplorationTest().catch((err) => {
  console.error('Test Failed:', err);
  process.exit(1);
});