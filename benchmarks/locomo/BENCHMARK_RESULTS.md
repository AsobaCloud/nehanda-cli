# LoCoMo Benchmark Results

Generated runs are written to `benchmarks/results/locomo_aimee_{direct,llm}_v<git-sha>.json`.
The checked direct baseline is `benchmarks/results/direct_baseline_v366fac9.json`.

Current direct baseline (`366fac9`):

- Overall: `1977` questions, `MRR=0.1772`, `Recall@5=0.2097`, `Recall@10=0.3218`
- Retrieval latency: `p50=12.072ms`, `p95=28.694ms`, `p99=34.331ms`, `min=3.244ms`, `max=41.919ms`
- Miss summary: `total=1477`, `ranking=631`, `granularity=460`, `entity=295`, `semantic=34`, `state=40`, `missing=17`
- Cat 1: `282` questions, `MRR=0.2501`, `Recall@5=0.1689`, `Recall@10=0.3033`, `p95=23.050ms`
- Cat 2: `321` questions, `MRR=0.1953`, `Recall@5=0.2138`, `Recall@10=0.4065`, `p95=25.364ms`
- Cat 3: `96` questions, `MRR=0.2013`, `Recall@5=0.1924`, `Recall@10=0.3103`, `p95=29.042ms`
- Cat 4: `841` questions, `MRR=0.1937`, `Recall@5=0.2739`, `Recall@10=0.3868`, `p95=29.264ms`
- Cat 5: `437` questions, `MRR=0.0799`, `Recall@5=0.1133`, `Recall@10=0.1487`, `p95=29.793ms`

Recompute the published breakdown from raw artefacts with:

```bash
python3 benchmarks/verify_scores.py benchmarks/results/locomo_aimee_direct_v<git-sha>.json
```

## BM25 Parity and Published-Score Calibration

### Anchor

The external calibration anchor is the **TrueMemory BM25 baseline** reported at
**80.5% Cat 1-4 accuracy** in the LoCoMo paper. All future aimee lift claims
against BM25 must cite this anchor rather than only internal history.

### Judge and Prompt Assumptions

The LLM-track harness deviates from TrueMemory's evaluation in the following ways:

| Dimension | TrueMemory | aimee harness |
|-----------|-----------|---------------|
| Judge model | GPT-4 (paper) | Configured execute-role agent (`~/.config/aimee/agents.json`) |
| Judge runs | 1 (paper) | 3 (majority vote) |
| Answer prompt | Paper-specific | `benchmarks/common/llm_eval.py::ANSWER_SYSTEM` |
| Top-K retrieval | Varies | `--top-k 100` (default) |
| Dataset version | LoCoMo-10 | LoCoMo-10 (`locomo10.json`) |

These gaps mean a direct numeric comparison is approximate. Variance over 1pp
versus the 80.5% anchor must be explained by documenting which of the above
factors is responsible.

### Running the Calibration Check

```bash
# Run BM25 on the full LoCoMo dataset
./benchmarks/run-llm.sh --systems bm25

# Compare all checked result files for this dataset
python3 benchmarks/verify_scores.py benchmarks/results/locomo_bm25_llm_v*.json
```

After a run, record the Cat 1-4 overall accuracy here and note any delta from
the 80.5% anchor with a brief causal explanation (e.g. judge-model gap, prompt
wording, top-K difference).

### Status

Pending first full BM25 LLM run on the canonical dataset.  Once run, record:

- `system_version` from the result JSON
- Cat 1, 2, 3, 4 accuracy
- Delta from 80.5% anchor and explanation
