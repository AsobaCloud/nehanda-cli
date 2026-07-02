#!/usr/bin/env python3
"""Parallel-supervisor SWE-bench benchmark.

Runs each SWE-bench Lite instance in up to three arms and measures how much of the
EXPENSIVE SUPERVISOR's token spend aimee offloads onto (free) delegate workers,
and whether it does so WITHOUT the wall-clock penalty that a single serial worker
pays (the Reddit result: -75.5% supervisor tokens but 3.75x slower).

  A  primary_alone       — the primary agent (supervisor) solves the task itself.
  B  supervised_serial   — supervisor + ONE delegate worker (--parallel 1).
  C  supervised_parallel — supervisor + N delegate workers (--parallel N).

Supervisor tokens are read from the aimee token ledger via
`aimee session tokens <sid> --json` (token_audit rows split by delegation_id:
empty = supervisor's own turns, set = delegate workers). Each instance/arm runs
under its OWN supervisor session_id so the split stays per-instance even when
instances run concurrently. The integrated patch is captured as
`git -C <cwd> diff <base_commit>` from the shared coord-job worktree, then graded
by the official SWE-bench Docker harness — the sole `resolved` source for ALL arms.

This module owns orchestration + record-keeping. The token/speed math and the
Reddit-style table live in supervised_report.py. Grading reuses the official
harness invocation from bench_swebench.py.

Environment:
  AIMEE_BENCH_FAKE_AGENT=1   — synthesize arm outputs (no live aimee), for CI.
  AIMEE_BENCH_FAKE_GRADER=1  — skip the SWE-bench Docker grader (resolved=None).
  AIMEE_BENCH_SUPERVISOR_N   — default N for arm C (default 6).
  AIMEE_BENCH_ARMS           — comma list, default "A,B,C".
"""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
import time
from pathlib import Path
from typing import Any

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from benchmarks.coding import supervised_report
from benchmarks.coding.bench_swebench import _extract_patch, _load_dataset, _build_prediction, _write_predictions

_FAKE_AGENT = os.environ.get("AIMEE_BENCH_FAKE_AGENT") == "1"
_FAKE_GRADER = os.environ.get("AIMEE_BENCH_FAKE_GRADER") == "1"

# The exact 10 SWE-bench Lite instances the Reddit post measured, for a direct
# head-to-head. A run may instead take a fresh random sample (see --sample).
REDDIT_INSTANCES = [
    "pytest-dev__pytest-11143",
    "scikit-learn__scikit-learn-13439",
    "sympy__sympy-20212",
    "django__django-12908",
    "pytest-dev__pytest-6116",
    "django__django-13447",
    "django__django-15814",
    "django__django-11179",
    "sympy__sympy-13480",
    "scikit-learn__scikit-learn-13584",
]


def _session_id(instance_id: str, arm: str, compaction: bool) -> str:
    """A unique supervisor session per (instance, arm, compaction) cell so the
    token_audit split is attributable even when cells run concurrently."""
    c = "cmp" if compaction else "raw"
    return f"swebench-sup-{instance_id}-{arm}-{c}"


class SupervisorClient:
    """Thin wrapper over the `aimee` CLI for the supervised benchmark.

    Concrete transport (how the primary agent is driven so its turns land in
    token_audit with delegation_id empty) is resolved by roundtable ruling Q1 and
    wired in `solve_direct` / `orchestrate`. Everything else — reading the token
    split, capturing the patch — is transport-independent and pinned here.
    """

    def __init__(self, aimee_bin: str, cwd: Path) -> None:
        self.aimee = aimee_bin
        self.cwd = cwd

    def _cli(self, args: list[str], *, timeout: float = 120.0) -> str:
        proc = subprocess.run(
            [self.aimee, *args],
            cwd=str(self.cwd),
            capture_output=True,
            text=True,
            timeout=timeout,
        )
        if proc.returncode != 0:
            raise RuntimeError(f"aimee {' '.join(args)} failed: {proc.stderr[:400]}")
        return proc.stdout

    def session_tokens(self, session_id: str) -> dict[str, Any]:
        """Read the supervisor-vs-worker token split for a session."""
        out = self._cli(["--json", "session", "tokens", session_id])
        return json.loads(out)

    def git_diff(self, base_commit: str) -> str:
        """Integrated patch = diff of the shared coord worktree vs base_commit."""
        proc = subprocess.run(
            ["git", "-C", str(self.cwd), "diff", base_commit],
            capture_output=True,
            text=True,
            timeout=120,
        )
        return proc.stdout


def _tokens_from_split(split: dict[str, Any]) -> dict[str, int]:
    sup = split.get("supervisor", {})
    wrk = split.get("worker", {})
    return {
        "supervisor_input_tokens": int(sup.get("prompt_tokens", 0)),
        "supervisor_output_tokens": int(sup.get("completion_tokens", 0)),
        "worker_input_tokens": int(wrk.get("prompt_tokens", 0)),
        "worker_output_tokens": int(wrk.get("completion_tokens", 0)),
    }


def _fake_record(instance_id: str, arm: str, compaction: bool, idx: int) -> dict[str, Any]:
    """Deterministic synthetic record so the harness + report are testable without
    a live aimee. Encodes the expected SHAPE: arm A pays full supervisor cost, arms
    B/C offload most of it onto workers, arm C runs fastest via parallelism."""
    base_sup_in = 12000 + idx * 500
    base_sup_out = 2200 + idx * 50
    comp_factor = 0.7 if compaction else 1.0  # S4 compaction trims supervisor input
    if arm == "A":
        sup_in, sup_out = int(base_sup_in * comp_factor), base_sup_out
        wrk_in = wrk_out = 0
        wall = 130.0 + idx * 8
        n = 1
    elif arm == "B":
        sup_in, sup_out = int(base_sup_in * 0.30 * comp_factor), int(base_sup_out * 0.45)
        wrk_in, wrk_out = int(base_sup_in * 0.85), int(base_sup_out * 1.4)
        wall = 320.0 + idx * 10  # serial single worker: SLOW (the Reddit tradeoff)
        n = 1
    else:  # C
        sup_in, sup_out = int(base_sup_in * 0.18 * comp_factor), int(base_sup_out * 0.38)
        wrk_in, wrk_out = int(base_sup_in * 0.95), int(base_sup_out * 1.6)
        wall = 88.0 + idx * 4  # parallel workers: FAST
        n = int(os.environ.get("AIMEE_BENCH_SUPERVISOR_N", "6"))
    return {
        "instance_id": instance_id,
        "arm": arm,
        "compaction": compaction,
        "supervisor_input_tokens": sup_in,
        "supervisor_output_tokens": sup_out,
        "worker_input_tokens": wrk_in,
        "worker_output_tokens": wrk_out,
        "wall_s": wall,
        "resolved": True if not _FAKE_GRADER else None,
        "n_workers": n,
        "patch": "" if _FAKE_GRADER or arm == "A" else "diff --git a/x b/x\n",
        "invalid": False,
    }


def run_arm(
    inst: dict[str, Any],
    arm: str,
    compaction: bool,
    idx: int,
    *,
    client: SupervisorClient | None,
    parallel_n: int,
) -> dict[str, Any]:
    """Execute one arm for one instance; return a measurement record.

    In FAKE mode returns a synthetic record. The live path (client set) is wired
    per the roundtable transport ruling: drive the primary to solve (A) or
    orchestrate delegates (B/C) under a per-cell session_id, then read the token
    split and capture the integrated patch.
    """
    instance_id = inst.get("instance_id", "")
    if client is None:  # FAKE
        return _fake_record(instance_id, arm, compaction, idx)

    base_commit = inst.get("base_commit", "")
    sid = _session_id(instance_id, arm, compaction)
    t0 = time.perf_counter()
    # NOTE: the concrete drive-the-primary calls are filled in after the transport
    # roundtable (Q1). They must (a) run under session_id `sid`, (b) toggle the
    # compaction config, (c) for B/C use `delegate plan --launch --parallel N`.
    patch = _drive_arm(client, inst, arm, sid, compaction, parallel_n)
    wall = time.perf_counter() - t0

    split = client.session_tokens(sid)
    rec = {
        "instance_id": instance_id,
        "arm": arm,
        "compaction": compaction,
        "wall_s": round(wall, 1),
        "n_workers": parallel_n if arm == "C" else (1 if arm == "B" else 1),
        "patch": patch,
        "resolved": None,  # filled by the grader
        "invalid": False,
    }
    rec.update(_tokens_from_split(split))
    return rec


def _drive_arm(
    client: SupervisorClient,
    inst: dict[str, Any],
    arm: str,
    sid: str,
    compaction: bool,
    parallel_n: int,
) -> str:
    """Placeholder for the live supervisor drive. Raises until the transport
    ruling (Q1) is wired, so a live run fails loudly rather than silently
    producing empty patches. FAKE mode never reaches here."""
    raise NotImplementedError(
        "live supervisor transport not yet wired — pending roundtable Q1 ruling; "
        "run with AIMEE_BENCH_FAKE_AGENT=1 for the harness/report path"
    )


def _grade(records: list[dict[str, Any]], target: str) -> None:
    """Grade every arm's patch with the official SWE-bench Docker harness and fill
    `resolved` in place. Skipped under FAKE_GRADER."""
    if _FAKE_GRADER:
        return
    from benchmarks.coding.bench_swebench import _grade_with_harness

    graded = [r for r in records if not r.get("invalid") and r.get("patch")]
    if not graded:
        return
    preds = [_build_prediction(r["instance_id"], f"{target}-{r['arm']}", r["patch"]) for r in graded]
    out_dir = Path(os.environ.get("AIMEE_BENCH_OUT", "benchmarks/results"))
    preds_path = out_dir / "supervised_predictions.jsonl"
    _write_predictions(preds_path, preds)
    run_id = f"aimee_sup_{int(time.time())}"
    try:
        resolved_ids, _ = _grade_with_harness(preds_path, run_id)
    except RuntimeError as exc:
        print(f"WARNING: grader unavailable: {exc}", file=sys.stderr)
        return
    for r in graded:
        r["resolved"] = r["instance_id"] in resolved_ids


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--variant", choices=["lite", "verified"], default="lite")
    parser.add_argument("--arms", default=os.environ.get("AIMEE_BENCH_ARMS", "A,B,C"))
    parser.add_argument("--parallel", type=int, default=int(os.environ.get("AIMEE_BENCH_SUPERVISOR_N", "6")))
    parser.add_argument("--compaction", choices=["on", "off", "both"], default="on")
    parser.add_argument("--reddit-set", action="store_true", help="Use the exact 10 Reddit instances")
    parser.add_argument("--max-cases", type=int, default=0)
    parser.add_argument("--target", default="aimee-supervised")
    parser.add_argument("--aimee-bin", default=os.environ.get("AIMEE_BENCH_CLIENT", "./aimee"))
    parser.add_argument("--cwd", default=os.environ.get("AIMEE_BENCH_REPO_CWD", "."))
    parser.add_argument("--output", required=True)
    args = parser.parse_args()

    arms = [a.strip().upper() for a in args.arms.split(",") if a.strip()]
    compactions = {"on": [True], "off": [False], "both": [True, False]}[args.compaction]

    data_dir = Path(os.environ.get("AIMEE_BENCH_DATA_DIR", "data"))
    instances = _load_dataset(data_dir, args.variant, 0) if not _FAKE_AGENT else [
        {"instance_id": iid, "repo": "x/y", "base_commit": "HEAD"} for iid in REDDIT_INSTANCES
    ]
    if args.reddit_set:
        wanted = set(REDDIT_INSTANCES)
        instances = [i for i in instances if i.get("instance_id") in wanted]
    if args.max_cases > 0:
        instances = instances[: args.max_cases]

    client = None
    if not _FAKE_AGENT:
        client = SupervisorClient(args.aimee_bin, Path(args.cwd))

    records: list[dict[str, Any]] = []
    for idx, inst in enumerate(instances):
        for arm in arms:
            for compaction in compactions:
                pn = args.parallel if arm == "C" else 1
                rec = run_arm(inst, arm, compaction, idx, client=client, parallel_n=pn)
                records.append(rec)
                print(
                    f"  {inst.get('instance_id')} arm={arm} cmp={compaction} "
                    f"sup={rec.get('supervisor_input_tokens')}+{rec.get('supervisor_output_tokens')} "
                    f"wall={rec.get('wall_s')}s",
                    file=sys.stderr,
                )

    _grade(records, args.target)

    reports = {}
    for compaction in compactions:
        rep = supervised_report.build_report(records, compaction=compaction)
        reports["compaction_on" if compaction else "compaction_off"] = rep
    lever = supervised_report.compaction_lever(records, "C") if len(compactions) == 2 else None

    headline_rep = reports.get("compaction_on") or next(iter(reports.values()))
    markdown = supervised_report.render_markdown(headline_rep, reddit_baseline={"tokens": -75.5, "slowdown": 3.75})

    output = {
        "dataset": f"swebench_{args.variant}",
        "arms": arms,
        "parallel_n": args.parallel,
        "records": records,
        "reports": reports,
        "compaction_lever": lever,
        "markdown": markdown,
    }
    out_path = Path(args.output)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(json.dumps(output, indent=2))
    print("\n" + markdown, file=sys.stderr)
    print(f"written to {args.output}", file=sys.stderr)


if __name__ == "__main__":
    main()
