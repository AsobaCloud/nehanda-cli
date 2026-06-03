#!/usr/bin/env python3
"""MDL synthesis selection benchmark runner.

Reads fixtures.json from the same directory and validates the MDL
implementation against the four fixture shapes defined in the proposal.
Exit 0 if all pass, 1 otherwise.

Uses libzstd via ctypes to exactly match the C implementation in kb_mdl.c.
No third-party Python packages required.
"""
import collections
import ctypes
import json
import os
import sys

try:
    _zstd = ctypes.cdll.LoadLibrary("libzstd.so.1")
except OSError:
    try:
        _zstd = ctypes.cdll.LoadLibrary("libzstd.so")
    except OSError:
        print("ERROR: libzstd.so not found")
        sys.exit(1)

_zstd.ZSTD_compressBound.restype = ctypes.c_size_t
_zstd.ZSTD_compressBound.argtypes = [ctypes.c_size_t]
_zstd.ZSTD_compress.restype = ctypes.c_size_t
_zstd.ZSTD_compress.argtypes = [ctypes.c_void_p, ctypes.c_size_t,
                                  ctypes.c_void_p, ctypes.c_size_t,
                                  ctypes.c_int]
_zstd.ZSTD_isError.restype = ctypes.c_uint
_zstd.ZSTD_isError.argtypes = [ctypes.c_size_t]

ZSTD_LEVEL = 3
SEP = b"\x00\xff\x00\xff"
SEP_LEN = len(SEP)  # 4


def _compress_size(data):
    src_len = len(data)
    if src_len == 0:
        return 0
    bound = _zstd.ZSTD_compressBound(src_len)
    dst = (ctypes.c_char * bound)()
    result = _zstd.ZSTD_compress(dst, bound, data, src_len, ZSTD_LEVEL)
    if _zstd.ZSTD_isError(result):
        raise RuntimeError("ZSTD_compress failed")
    return int(result)


def mdl_score(candidate, evidence):
    """Two-part MDL score matching kb_mdl_score() in src/kb/kb_mdl.c."""
    cb = candidate.encode()
    eb = evidence.encode()
    lc = _compress_size(cb)
    lce = _compress_size(cb + SEP + eb)
    residual = max(0.0, lce - lc - SEP_LEN)
    return {"l_candidate": float(lc), "l_residual": residual, "total": lc + residual}


def mdl_winner(candidates, evidence):
    """Return index of candidate with lowest MDL total."""
    scores = [mdl_score(c, evidence)["total"] for c in candidates]
    return scores.index(min(scores))


def run_fixture(fx):
    shape = fx["shape"]

    if shape in ("positive", "redundancy"):
        cands = [c["text"] for c in fx["candidates"]]
        evidence = fx["evidence_bundle"]
        winner_idx = mdl_winner(cands, evidence)
        winner_id = fx["candidates"][winner_idx]["id"]
        expected = fx["expected_winner"]
        ok = winner_id == expected
        detail = "winner={} expected={}".format(winner_id, expected)
        return ok, detail

    if shape == "compression_resistant":
        cands = fx["candidates"]
        clusters = [c.get("agreement_cluster", "") for c in cands]
        counts = collections.Counter(c for c in clusters if c)
        evidence = fx["evidence_bundle"]
        if counts:
            majority_cluster, majority_count = counts.most_common(1)[0]
            cluster_idxs = [i for i, c in enumerate(clusters) if c == majority_cluster]
            if majority_count >= 2:
                cluster_texts = [cands[i]["text"] for i in cluster_idxs]
                local_winner = mdl_winner(cluster_texts, evidence)
                winner_id = cands[cluster_idxs[local_winner]]["id"]
            else:
                winner_id = cands[0]["id"]
        else:
            texts = [c["text"] for c in cands]
            winner_id = cands[mdl_winner(texts, evidence)]["id"]
        expected = fx["expected_winner"]
        ok = winner_id == expected
        detail = "winner={} expected={}".format(winner_id, expected)
        return ok, detail

    if shape == "prompt_bump_drift":
        pre = [c["text"] for c in fx["pre_bump_candidates"]]
        post = [c["text"] for c in fx["post_bump_candidates"]]
        evidence = fx["evidence_bundle"]
        threshold = fx.get("threshold", 0.30)

        pre_winner_idx = mdl_winner(pre, evidence)
        post_winner_idx = mdl_winner(post, evidence)
        pre_lc = mdl_score(pre[pre_winner_idx], evidence)["l_candidate"]
        post_lc = mdl_score(post[post_winner_idx], evidence)["l_candidate"]

        if pre_lc <= 0:
            return False, "pre_lc=0 (degenerate)"

        drift = (post_lc - pre_lc) / pre_lc
        alert_fired = drift >= threshold
        expected_alert = fx["expected_drift_alert"]
        ok = alert_fired == expected_alert
        detail = "drift={:.3f} threshold={} alert={} expected={}".format(
            drift, threshold, alert_fired, expected_alert)
        return ok, detail

    return False, "unknown shape: {}".format(shape)


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    fixture_path = os.path.join(here, "fixtures.json")
    with open(fixture_path) as f:
        data = json.load(f)

    fixtures = data["fixtures"]
    passed = 0
    failed = 0

    for fx in fixtures:
        try:
            ok, detail = run_fixture(fx)
        except Exception as exc:
            ok, detail = False, "exception: {}".format(exc)
        status = "PASS" if ok else "FAIL"
        print("  {}  {}  [{}]  {}".format(status, fx["id"], fx["shape"], detail))
        if ok:
            passed += 1
        else:
            failed += 1

    print("\n{}/{} fixtures passed.".format(passed, passed + failed))
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
