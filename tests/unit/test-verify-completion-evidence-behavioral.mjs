import assert from 'node:assert/strict';
import { WorkflowState, setPhase } from '../../lib/workflow.mjs';

/**
 * HP-8 & HP-10: Completion Evidence & Safety Gate Verification
 * Enforces AGENTS 7–8 and AGENTS 8–9 boundary requirements.
 */
async function runTests() {
  console.log('--- Running test-verify-completion-evidence-behavioral.mjs ---');

  // Scenario 1: Non-zero safety checker exit code blocks transition to verify/done
  {
    const state = new WorkflowState({ currentPhase: 'test' });
    const mockSafetyResults = [
      { checker: 'js-safety-checker.cjs', command: 'node utilities/js-safety-checker.cjs', exit_code: 0, output: 'OK' },
      { checker: 'shell-safety-checker.sh', command: './utilities/shell-safety-checker.sh', exit_code: 1, output: 'Syntax error' },
      { checker: 'python-safety-checker.py', command: 'python3 utilities/python-safety-checker.py', exit_code: 0, output: 'OK' },
    ];
    
    state.recordSafetyResults(mockSafetyResults);
    state.setPathCoverage(85); // High coverage, but 1 checker failed

    const transitionResult = setPhase(state, 'verify');
    assert.equal(transitionResult.success, false, 'Transition to verify must fail when safety checker exits non-zero');
    assert.equal(state.currentPhase, 'test', 'Phase must remain locked in test');
    assert.match(
      state.lastRejectionReason,
      /AGENTS 7–8: GREEN pass, 3\/3 safety checker clearance, and >=70% path coverage required\./,
      'Must emit exact AGENTS 7–8 rejection string'
    );
    console.log('✔ Passed Scenario 1: Non-zero safety exit code blocks phase promotion.');
  }

  // Scenario 2: Path coverage under 70% blocks completion offer
  {
    const state = new WorkflowState({ currentPhase: 'test' });
    const mockSafetyResults = [
      { checker: 'js-safety-checker.cjs', command: 'node utilities/js-safety-checker.cjs', exit_code: 0, output: 'OK' },
      { checker: 'shell-safety-checker.sh', command: './utilities/shell-safety-checker.sh', exit_code: 0, output: 'OK' },
      { checker: 'python-safety-checker.py', command: 'python3 utilities/python-safety-checker.py', exit_code: 0, output: 'OK' },
    ];
    
    state.recordSafetyResults(mockSafetyResults);
    state.setPathCoverage(65); // Coverage is 65% (< 70%)

    const transitionResult = setPhase(state, 'verify');
    assert.equal(transitionResult.success, false, 'Transition to verify must fail when path coverage < 70%');
    assert.equal(state.currentPhase, 'test', 'Phase must remain locked in test');
    assert.match(
      state.lastRejectionReason,
      /AGENTS 7–8/,
      'Rejection reason must reference AGENTS 7–8 boundary mandate'
    );
    console.log('✔ Passed Scenario 2: Coverage < 70% suppresses completion gate.');
  }

  // Scenario 3: 3/3 safety pass AND >=70% coverage allows transition to verify
  {
    const state = new WorkflowState({ currentPhase: 'test' });
    const mockSafetyResults = [
      { checker: 'js-safety-checker.cjs', command: 'node utilities/js-safety-checker.cjs', exit_code: 0, output: 'OK' },
      { checker: 'shell-safety-checker.sh', command: './utilities/shell-safety-checker.sh', exit_code: 0, output: 'OK' },
      { checker: 'python-safety-checker.py', command: 'python3 utilities/python-safety-checker.py', exit_code: 0, output: 'OK' },
    ];
    
    state.recordSafetyResults(mockSafetyResults);
    state.setPathCoverage(75);

    const transitionResult = setPhase(state, 'verify');
    assert.equal(transitionResult.success, true, 'Transition to verify should succeed with clean safety + 75% coverage');
    assert.equal(state.currentPhase, 'verify', 'Phase must advance to verify');
    console.log('✔ Passed Scenario 3: 3/3 safety pass + 75% coverage allows transition to verify.');
  }

  console.log('All verify-completion-evidence tests passed successfully.');
}

runTests().catch((err) => {
  console.error('Test Failed:', err);
  process.exit(1);
});