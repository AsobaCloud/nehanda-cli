import assert from 'node:assert/strict';
import { WorkflowState, setPhase, MANDATE_REJECT_REASONS } from '../../lib/workflow.mjs';

/**
 * HP-7: Mandatory Rejection Reason Mapping Across All 8 Boundary Gates
 */
async function runTests() {
  console.log('--- Running test-transition-mandate-messages-behavioral.mjs ---');

  const boundaryCases = [
    { from: 'idle', to: 'explore', expectedReason: MANDATE_REJECT_REASONS['idle->explore'] },
    { from: 'explore', to: 'planning', expectedReason: MANDATE_REJECT_REASONS['explore->planning'] },
    { from: 'planning', to: 'test', expectedReason: MANDATE_REJECT_REASONS['planning->test'] },
    { from: 'test', to: 'implement', expectedReason: MANDATE_REJECT_REASONS['test->implement'] },
    { from: 'implement', to: 'test', expectedReason: MANDATE_REJECT_REASONS['implement->test'] },
    { from: 'test', to: 'verify', expectedReason: MANDATE_REJECT_REASONS['test->verify'] },
    { from: 'verify', to: 'done', expectedReason: MANDATE_REJECT_REASONS['verify->done'] },
    { from: 'done', to: 'idle', expectedReason: MANDATE_REJECT_REASONS['done->idle'] },
  ];

  for (const tc of boundaryCases) {
    const state = new WorkflowState({ currentPhase: tc.from });
    // Intentionally omit prerequisites to trigger mandate failure
    const result = setPhase(state, tc.to);

    assert.equal(result.success, false, `Unsatisfied transition from ${tc.from} to ${tc.to} must be rejected`);
    assert.equal(state.currentPhase, tc.from, `Phase must remain in ${tc.from} upon rejection`);
    assert.equal(
      state.lastRejectionReason,
      tc.expectedReason,
      `Rejection reason for ${tc.from}->${tc.to} must match exact mandate string:\nExpected: ${tc.expectedReason}\nActual:   ${state.lastRejectionReason}`
    );
    console.log(`✔ Boundary ${tc.from} -> ${tc.to} correctly mapped to mandate string.`);
  }

  console.log('All mandate boundary rejection message tests passed successfully.');
}

runTests().catch((err) => {
  console.error('Test Failed:', err);
  process.exit(1);
});