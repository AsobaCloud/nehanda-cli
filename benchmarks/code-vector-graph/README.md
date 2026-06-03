# Effectiveness-Weighted Code Vector Graph, Phase 7 evaluation

This directory holds the Phase 7 feedback/shadow-evaluation artifacts for the
effectiveness-weighted code vector graph (effectiveness-weighted ranking over the
code call/dependency graph).

## Files

- `ablation-matrix.json`, the ablation arms (baseline → full fusion) with the
  config gates each arm sets, the metrics to record, and the provisional
  promotion gate. Each arm is run against the production corpus.
- `production-corpus.json`, the production-gate retrieval corpus (≥100
  queries) across the required topic categories. `expected_ids` are
  owner-populated against a live indexed corpus; `code_shaped` is the
  deterministic Phase 6 shape expectation, enforced by `validate-corpus.py`.
- `validate-corpus.py`, structural validation: ≥100 queries, all categories
  present, every entry well-formed. Run in CI/verify as a cheap guard.

## Running the ablation matrix

The eval owner runs each arm with its config gates applied, against a populated
DB2 instance with code projection synced:

```
for arm in $(jq -r '.arms[].name' ablation-matrix.json); do
  # apply arm config, then:
  aimee memory benchmark --corpus production-corpus.json --arm "$arm" \
    --metrics recall@1,recall@5,recall@10,mrr,ndcg@10,citation_accuracy
done
```

Shadow mode (`memory.recall.graph_code_fusion_state=shadow`) records fused rank
deltas into `memory_recall_shadow_deltas` while returning the fusion-off order
unchanged, so the deltas can be analysed before promotion. Retention is bounded
by `memory.recall.shadow_delta_max_rows` (default 10000) and
`memory.recall.shadow_delta_retention_days` (default 14), enforced by
`db2_shadow_delta_cleanup()`.

## Promotion gate

Fused ranking is promoted to default only after the `full_fusion` arm beats the
`baseline` arm on the agreed gates (provisional: Recall@10 +15pp, MRR +0.10)
while p95 recall latency stays within 25% overhead. The implementation PR may
replace the provisional metric gate with an explicit precision/recall tradeoff
and non-regression bound after measuring the live baseline.
