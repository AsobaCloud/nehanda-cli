#!/usr/bin/env python3
"""Unit tests for the supervised SWE-bench benchmark + report.

Exercises the token/speed math and the fake-mode harness records without a live
aimee, Docker, or network. Run: python3 -m pytest benchmarks/tests/test_bench_swebench_supervised.py
"""

from __future__ import annotations

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from benchmarks.coding import supervised_report as R


def _cell(inst, arm, cmp, sup_in, sup_out, wrk_in=0, wrk_out=0, wall=100.0, resolved=True, invalid=False):
    return {
        "instance_id": inst,
        "arm": arm,
        "compaction": cmp,
        "supervisor_input_tokens": sup_in,
        "supervisor_output_tokens": sup_out,
        "worker_input_tokens": wrk_in,
        "worker_output_tokens": wrk_out,
        "wall_s": wall,
        "resolved": resolved,
        "invalid": invalid,
    }


class TestPctReduction(unittest.TestCase):
    def test_saving_is_positive(self):
        self.assertEqual(R.pct_reduction(100, 25), 75.0)

    def test_increase_is_negative(self):
        self.assertEqual(R.pct_reduction(100, 120), -20.0)

    def test_zero_baseline_is_none(self):
        self.assertIsNone(R.pct_reduction(0, 5))


class TestArmPairing(unittest.TestCase):
    def test_micro_aggregate_is_sum_based_not_mean_of_ratios(self):
        # Two instances with very different token magnitudes: the sum-based
        # aggregate must weight by total tokens, not average the percentages.
        recs = [
            _cell("big", "A", True, 100000, 10000),
            _cell("big", "C", True, 10000, 5000, wrk_in=90000, wrk_out=5000),  # 90% in, 50% out
            _cell("small", "A", True, 100, 100),
            _cell("small", "C", True, 90, 90, wrk_in=10, wrk_out=10),  # 10% in, 10% out
        ]
        rep = R.build_report(recs, compaction=True)
        cmp = rep["comparisons"]["C_vs_A"]
        # Micro input reduction: (100100 - 10090) / 100100 ~ 89.9% (dominated by big).
        self.assertGreater(cmp["micro_supervisor_input_reduction_pct"], 89.0)
        # Macro (mean of per-task ratios) would be ~(90+10)/2 = 50% — must differ.
        self.assertLess(cmp["macro_supervisor_input_reduction_pct"], 60.0)

    def test_pairs_only_shared_instances(self):
        recs = [
            _cell("i1", "A", True, 1000, 200),
            _cell("i1", "C", True, 200, 100, wrk_in=900, wrk_out=150),
            _cell("i2", "A", True, 1000, 200),  # no C for i2 -> excluded from pairing
        ]
        cmp = R.build_report(recs, compaction=True)["comparisons"]["C_vs_A"]
        self.assertEqual(cmp["n_instances"], 1)

    def test_invalid_records_excluded(self):
        recs = [
            _cell("i1", "A", True, 1000, 200),
            _cell("i1", "C", True, 200, 100, wrk_in=900, wrk_out=150, invalid=True),
        ]
        cmp = R.build_report(recs, compaction=True)["comparisons"].get("C_vs_A")
        # C cell is invalid -> no valid pair.
        self.assertTrue(cmp is None or cmp["n_instances"] == 0)


class TestTotalLlmHonesty(unittest.TestCase):
    def test_total_llm_shows_compute_moved_not_vanished(self):
        # Supervisor drops hugely but workers pick up the compute: total-LLM
        # reduction should be modest/negative even though supervisor reduction is large.
        recs = [
            _cell("i", "A", True, 10000, 2000),
            _cell("i", "C", True, 1500, 700, wrk_in=9000, wrk_out=3000),
        ]
        cmp = R.build_report(recs, compaction=True)["comparisons"]["C_vs_A"]
        self.assertGreater(cmp["micro_supervisor_total_reduction_pct"], 75.0)  # big supervisor win
        # total-LLM went UP (compute moved onto free workers) -> reduction negative.
        self.assertLess(cmp["total_llm_reduction_pct"], 0.0)


class TestSpeed(unittest.TestCase):
    def test_parallel_beats_serial_and_solo(self):
        recs = []
        for i in range(3):
            recs.append(_cell(f"i{i}", "A", True, 1000, 200, wall=130.0))
            recs.append(_cell(f"i{i}", "B", True, 300, 90, wrk_in=800, wrk_out=250, wall=330.0))
            recs.append(_cell(f"i{i}", "C", True, 180, 76, wrk_in=900, wrk_out=280, wall=90.0))
        rep = R.build_report(recs, compaction=True)
        # C faster than A (solo) and much faster than serial B.
        self.assertLess(rep["speed"]["C_over_A_walltime_ratio"], 1.0)
        self.assertGreater(rep["speed"]["parallel_speedup_vs_serial"], 3.0)


class TestCompactionLever(unittest.TestCase):
    def test_marginal_effect_isolated_per_instance(self):
        recs = [
            _cell("i", "C", True, 700, 100, wrk_in=900, wrk_out=300),   # compaction on
            _cell("i", "C", False, 1000, 100, wrk_in=900, wrk_out=300),  # compaction off
        ]
        lever = R.compaction_lever(recs, "C")
        self.assertEqual(lever["n_instances"], 1)
        self.assertEqual(lever["compaction_input_reduction_pct"], 30.0)  # 1000 -> 700


class TestResolutionParity(unittest.TestCase):
    def test_grader_is_sole_resolved_source(self):
        recs = [
            _cell("i1", "A", True, 1000, 200, resolved=True),
            _cell("i2", "A", True, 1000, 200, resolved=False),
        ]
        res = R.build_report(recs, compaction=True)["resolution"]["A"]
        self.assertEqual(res["graded"], 2)
        self.assertEqual(res["resolved"], 1)
        self.assertEqual(res["resolve_rate"], 0.5)

    def test_ungraded_none_not_counted(self):
        recs = [
            _cell("i1", "A", True, 1000, 200, resolved=None),
        ]
        res = R.build_report(recs, compaction=True)["resolution"]["A"]
        self.assertEqual(res["graded"], 0)


class TestFakeHarnessShape(unittest.TestCase):
    def test_fake_records_have_expected_arm_shape(self):
        import os

        os.environ["AIMEE_BENCH_FAKE_AGENT"] = "1"
        os.environ["AIMEE_BENCH_FAKE_GRADER"] = "1"
        from importlib import reload
        from benchmarks.coding import bench_swebench_supervised as B

        reload(B)
        a = B._fake_record("x", "A", True, 0)
        c = B._fake_record("x", "C", True, 0)
        # Arm A pays full supervisor cost, no workers; arm C offloads to workers.
        self.assertEqual(a["worker_input_tokens"], 0)
        self.assertGreater(c["worker_input_tokens"], 0)
        self.assertLess(c["supervisor_input_tokens"], a["supervisor_input_tokens"])
        # Arm C runs faster than arm B (parallel vs serial).
        b = B._fake_record("x", "B", True, 0)
        self.assertLess(c["wall_s"], b["wall_s"])


if __name__ == "__main__":
    unittest.main()
