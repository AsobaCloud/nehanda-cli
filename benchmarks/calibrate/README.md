# Promotion Calibration Fixtures

These fixtures exercise the Bayesian promotion-threshold calibration sidecar.
Each file is a standalone sidecar request:

```sh
python3 scripts/calibration-sidecar.py < benchmarks/calibrate/positive-memory.json
```

The fixture set covers:

- `positive-memory.json`: ordinary memory promotion evidence with a strong
  high-confidence acceptance bucket.
- `false-positive-conformal.json`: high rejected-confidence rows that force a
  conservative conformal floor.
- `prompt-bump-reconverge.json`: a fresh prompt version with thinner evidence,
  used to verify conservative re-convergence after a prompt change.
