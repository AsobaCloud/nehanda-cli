# Neural Guardrails Phase 2b Report

Command:

```sh
python3 tools/guardrails_replay.py --fixtures benchmarks/guardrails/fixtures
```

Committed fixture corpus:

- Total fixtures: 55
- Yellow-zone deterministic-allow fixtures: 35
- Benign fixtures: 10
- High-risk regression fixtures: 10

Replay metrics from the committed fixture scores:

- Precision for `warn`-or-higher recommendations: 100.00%
- Deterministic-only recall on yellow-zone fixtures: 0.00%
- Advisory recall on yellow-zone fixtures: 100.00%
- Recall improvement: 100.00%

Gate result: pass. The replay exceeds the Phase 2b target of at least
20% yellow-zone recall improvement and at least 0.80 precision across
the full fixture set.
