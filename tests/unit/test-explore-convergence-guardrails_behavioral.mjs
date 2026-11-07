import assert from 'node:assert/strict';

const { buildExploreUserMessage } = await import('../../lib/orchestrate.mjs');

function section(label, fn) {
  fn();
  console.log(`  ✓ ${label}`);
}

console.log('[explore-convergence-guardrails] RED target: required exploration protocols\n');

const text = buildExploreUserMessage('Review the health check service running on asobacorp-1:3000.');

section('registry-first instruction preserved', () => {
  assert.match(text, /Read docs\/MAP\.md first/i);
});

section('broad whole-file sweeps prohibited', () => {
  assert.match(text, /Whole-file sweeps are prohibited/i);
});

section('dedup protocol is explicit', () => {
  assert.match(text, /Do not re-read a file or repeat a search after no new information/i);
});

section('unbounded search patterns prohibited', () => {
  assert.match(text, /Unbounded patterns such as/i);
  assert.match(text, /\.\+/);
});

section('exploration exhaustion handoff is explicit', () => {
  assert.match(text, /exploration exhausted/i);
  assert.match(text, /Exploration complete/i);
});

section('no-match failure mode is explicit', () => {
  assert.match(text, /No matching context in current repo; provide clarification or expanded scope\./);
});

console.log('\nSummary: all guardrail assertions passed.');
