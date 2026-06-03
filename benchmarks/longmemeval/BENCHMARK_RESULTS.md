# LongMemEval Benchmark Results

Generated runs are written to `benchmarks/results/longmemeval_aimee_{direct,llm}_v<git-sha>.json`.
The checked direct baseline is `benchmarks/results/direct_baseline_v366fac9.json`.
Direct benchmark payloads now also include `pagerank_comparison`, which records
PageRank off/on deltas for the `single-session-assistant` subset when that
subset exists in the evaluated dataset.

Current direct baseline (`366fac9`):

- Overall: `470` cases, `MRR=0.6593`, `Recall@5=0.4883`, `Recall@10=0.5233`
- Retrieval latency: `p50=12.432ms`, `p95=28.656ms`, `p99=34.355ms`, `min=5.433ms`, `max=46.916ms`
- Miss summary: `total=108`, `entity=86`, `temporal=11`, `semantic=2`, `state=4`, `missing=5`
- `knowledge-update`: `72` cases, `MRR=0.6729`, `Recall@5=0.3681`, `Recall@10=0.3889`, `p95=17.138ms`
- `multi-session`: `121` cases, `MRR=0.6870`, `Recall@5=0.3638`, `Recall@10=0.3700`, `p95=17.093ms`
- `single-session-assistant`: `56` cases, `MRR=0.8707`, `Recall@5=0.9286`, `Recall@10=0.9464`, `p95=37.988ms`
- `single-session-preference`: `30` cases, `MRR=0.3715`, `Recall@5=0.5667`, `Recall@10=0.7667`, `p95=24.050ms`
- `single-session-user`: `64` cases, `MRR=0.4560`, `Recall@5=0.5625`, `Recall@10=0.6094`, `p95=15.316ms`
- `temporal-reasoning`: `127` cases, `MRR=0.7024`, `Recall@5=0.4252`, `Recall@10=0.4580`, `p95=21.581ms`

Recompute the published breakdown from raw artefacts with:

```bash
python3 benchmarks/verify_scores.py benchmarks/results/longmemeval_aimee_direct_v<git-sha>.json
```
