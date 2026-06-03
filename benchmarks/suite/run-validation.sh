#!/usr/bin/env bash
# benchmarks/suite/run-validation.sh
# Operator rollout-validation runbook for the unified benchmark suite.
#
# Capability-aware: it runs every track this host *can* run and cleanly SKIPS
# (with a reason) the ones whose infrastructure is absent, then prints a summary.
# It only fails if a track that should have run actually errored. So the runbook
# is runnable on any host — a laptop with no Docker, a GPU box, or CI — and tells
# you exactly what each environment did.
#
# Steps: 1 provisioning preflight · 2 end-to-end tracks · 3 determinism ·
#        4 cross-target comparison report · 5 smoke budget (<15 min).
#
# Capability gates:
#   - memory benches need their dataset provisioned (check_provisioning.py).
#   - coding benches (swebench*/terminalbench/aider_polyglot) need Docker.
#   - the `aimee` target does memory store/search against a vector DB; to avoid
#     polluting the operator's OPERATIONAL memory it is SKIPPED unless
#     AIMEE_BENCH_ALLOW_AIMEE=1 is set (point AIMEE_BENCH_SOURCE_HOME at a scratch
#     home backed by an isolated DB first — see benchmarks/PROVISIONING.md).
#
# With AIMEE_BENCH_FAKE_AGENT=1 the whole runbook runs with no external models.
#
# Usage:
#   benchmarks/suite/run-validation.sh \
#       [--benches locomo,longmemeval_s] [--targets model_only] \
#       [--baseline rag_chromadb] [--det-target model_only] [--seed 42] \
#       [--max-samples N] [--max-questions N] [--skip-determinism] [--skip-smoke]
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SUITE_DIR="$ROOT_DIR/benchmarks/suite"
RESULTS_DIR="${AIMEE_BENCH_RESULTS_DIR:-$ROOT_DIR/benchmarks/results}"
GIT_SHA="$(git -C "$ROOT_DIR" rev-parse --short HEAD)"

BENCHES="locomo,longmemeval_s"
TARGETS="model_only"
BASELINE="bm25"   # dependency-free non-aimee baseline (rag_chromadb needs chromadb)
DET_TARGET="model_only"
SEED="42"
DO_DETERMINISM=1
DO_SMOKE=1
SMOKE_BUDGET_S=900   # 15 minutes

while [[ $# -gt 0 ]]; do
  case "$1" in
    --benches)          BENCHES="$2"; shift 2 ;;
    --targets)          TARGETS="$2"; shift 2 ;;
    --baseline)         BASELINE="$2"; shift 2 ;;
    --det-target)       DET_TARGET="$2"; shift 2 ;;
    --seed)             SEED="$2"; shift 2 ;;
    --max-samples)      export AIMEE_BENCH_MAX_SAMPLES="$2"; shift 2 ;;
    --max-questions)    export AIMEE_BENCH_MAX_QUESTIONS="$2"; shift 2 ;;
    --skip-determinism) DO_DETERMINISM=0; shift ;;
    --skip-smoke)       DO_SMOKE=0; shift ;;
    -h|--help) grep '^#' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    *) echo "unknown option: $1" >&2; exit 1 ;;
  esac
done

ALL_TARGETS="$TARGETS"
[[ -n "$BASELINE" && ",$TARGETS," != *",$BASELINE,"* ]] && ALL_TARGETS="$TARGETS,$BASELINE"

export AIMEE_BENCH_RESULTS_DIR="$RESULTS_DIR"
mkdir -p "$RESULTS_DIR"

step() { echo ""; echo "=== $* ==="; }

RAN=(); SKIPPED=(); FAILED=()
note_skip() { echo "  SKIP $1 — $2"; SKIPPED+=("$1 ($2)"); }
note_ran()  { echo "  ran  $1"; RAN+=("$1"); }
note_fail() { echo "  FAIL $1" >&2; FAILED+=("$1"); }

is_coding() { case "$1" in swebench_lite|swebench_verified|terminalbench|aider_polyglot|humaneval|mbpp_plus|bigcodebench|repobench|livecodebench) return 0;; *) return 1;; esac; }
have_docker() { command -v docker >/dev/null 2>&1 && docker info >/dev/null 2>&1; }
have_fake()   { [[ "${AIMEE_BENCH_FAKE_AGENT:-0}" == "1" ]]; }

# Provisioning status for each dataset (id -> status), computed once.
declare -A PROV
while IFS=$'\t' read -r id status; do PROV["$id"]="$status"; done < <(
  python3 "$ROOT_DIR/benchmarks/check_provisioning.py" --json 2>/dev/null \
    | python3 -c "import sys,json
for k,v in json.load(sys.stdin)['datasets'].items(): print(f\"{k}\t{v['status']}\")"
)
dataset_ready() {
  local s="${PROV[$1]:-unknown}"
  [[ "$s" == provisioned_* || "$s" == bundled ]]
}

# can_run TARGET BENCH -> 0 runnable, else sets skip reason in $SKIP_REASON
can_run() {
  local target="$1" bench="$2"
  # Docker / dataset are real prerequisites a fake agent cannot supply.
  if is_coding "$bench" && ! have_docker; then
    SKIP_REASON="coding pillar needs Docker (none here)"; return 1
  fi
  if ! is_coding "$bench" && ! dataset_ready "$bench"; then
    SKIP_REASON="dataset not provisioned (status=${PROV[$bench]:-unknown}; run scripts/provision-benchmarks.sh)"; return 1
  fi
  # The aimee target writes to a vector DB. Fake-agent memory is in-process (no
  # persistence), so only the real path needs the isolated-DB opt-in.
  if [[ "$target" == "aimee" ]] && ! have_fake && [[ "${AIMEE_BENCH_ALLOW_AIMEE:-0}" != "1" ]]; then
    SKIP_REASON="aimee target writes to a vector DB; set AIMEE_BENCH_ALLOW_AIMEE=1 with an isolated scratch home to avoid polluting operational memory"; return 1
  fi
  return 0
}

# ---------------------------------------------------------------------------
step "1/5 provisioning preflight"
python3 "$ROOT_DIR/benchmarks/check_provisioning.py" --require-coverage

# ---------------------------------------------------------------------------
step "2/5 end-to-end tracks (targets: $ALL_TARGETS · benches: $BENCHES)"
IFS=',' read -r -a TARGET_ARR <<< "$ALL_TARGETS"
IFS=',' read -r -a BENCH_ARR <<< "$BENCHES"
for target in "${TARGET_ARR[@]}"; do
  for bench in "${BENCH_ARR[@]}"; do
    SKIP_REASON=""
    if ! can_run "$target" "$bench"; then
      note_skip "$target/$bench" "$SKIP_REASON"; continue
    fi
    if "$SUITE_DIR/run-all.sh" --target "$target" --bench "$bench"; then
      note_ran "$target/$bench"
    else
      note_fail "$target/$bench"
    fi
  done
done

# ---------------------------------------------------------------------------
if [[ "$DO_DETERMINISM" -eq 1 ]]; then
  step "3/5 determinism ($DET_TARGET, same bench+seed twice)"
  DET_BENCH="${BENCH_ARR[0]}"
  SKIP_REASON=""
  if ! can_run "$DET_TARGET" "$DET_BENCH"; then
    note_skip "determinism:$DET_TARGET/$DET_BENCH" "$SKIP_REASON"
  else
    det_ok=1
    for run in 1 2; do
      AIMEE_BENCH_RESULTS_DIR="$RESULTS_DIR/determinism/run$run" AIMEE_BENCH_SEED="$SEED" \
        "$SUITE_DIR/run-llm.sh" --target "$DET_TARGET" --bench "$DET_BENCH" || det_ok=0
    done
    if [[ "$det_ok" -eq 1 ]] && python3 "$ROOT_DIR/benchmarks/check_determinism.py" --strict \
        "$RESULTS_DIR"/determinism/run1/*.json "$RESULTS_DIR"/determinism/run2/*.json; then
      note_ran "determinism:$DET_TARGET/$DET_BENCH"
    else
      note_fail "determinism:$DET_TARGET/$DET_BENCH"
    fi
  fi
else
  step "3/5 determinism (skipped)"
fi

# ---------------------------------------------------------------------------
step "4/5 cross-target comparison report"
for bench in "${BENCH_ARR[@]}"; do
  files=()
  for target in "${TARGET_ARR[@]}"; do
    for track in llm direct; do
      f="$RESULTS_DIR/${bench}_${target}_${track}_v${GIT_SHA}.json"
      [[ -f "$f" ]] && files+=("$f")
    done
  done
  if [[ "${#files[@]}" -ge 2 ]]; then
    python3 "$ROOT_DIR/benchmarks/compare_targets.py" "${files[@]}" \
      --out "$RESULTS_DIR/comparison_${bench}_v${GIT_SHA}" && note_ran "comparison:$bench"
  else
    note_skip "comparison:$bench" "fewer than 2 target result files"
  fi
done

# ---------------------------------------------------------------------------
if [[ "$DO_SMOKE" -eq 1 ]]; then
  step "5/5 smoke budget (<$((SMOKE_BUDGET_S/60)) min)"
  start=$(date +%s)
  if AIMEE_BENCH_RESULTS_DIR="$RESULTS_DIR/smoke" "$SUITE_DIR/run-smoke.sh"; then
    elapsed=$(( $(date +%s) - start ))
    echo "smoke completed in ${elapsed}s (budget ${SMOKE_BUDGET_S}s)"
    if [[ "$elapsed" -gt "$SMOKE_BUDGET_S" ]]; then note_fail "smoke (over budget ${elapsed}s)"; else note_ran "smoke"; fi
  else
    note_fail "smoke"
  fi
else
  step "5/5 smoke budget (skipped)"
fi

# ---------------------------------------------------------------------------
step "summary"
echo "  ran:     ${#RAN[@]}    ${RAN[*]:-(none)}"
echo "  skipped: ${#SKIPPED[@]}    ${SKIPPED[*]:-(none)}"
echo "  failed:  ${#FAILED[@]}    ${FAILED[*]:-(none)}"
echo ""
echo "artifacts under $RESULTS_DIR"
[[ "${#FAILED[@]}" -eq 0 ]] || { echo "validation: ${#FAILED[@]} track(s) failed" >&2; exit 1; }
echo "validation: OK (skips are expected where infra is absent)"
