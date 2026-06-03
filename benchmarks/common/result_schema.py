#!/usr/bin/env python3
"""Schema validation helpers for benchmark result files."""

from __future__ import annotations

from typing import Any


# ---------------------------------------------------------------------------
# Per-row result schemas (unchanged — backward-compatible)
# ---------------------------------------------------------------------------

DIRECT_RESULT_REQUIRED = {
    "system": str,
    "track": str,
    "git_commit": str,
    "question_id": str,
    "question": str,
    "gold_answer": str,
    "verdict": str,
    "retrieval_latency_s": (int, float),
    "retrieved_ids": list,
    "citations": list,
}


LLM_REQUIRED = {
    **DIRECT_RESULT_REQUIRED,
    "generated_answer": str,
    "judge_votes": list,
    "answer_latency_s": (int, float),
    "judge_latency_s": (int, float),
    "wall_clock_s": (int, float),
    "tokens": dict,
    "cost": dict,
}


def _ensure(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def _validate_required(row: dict[str, Any], required: dict[str, Any]) -> None:
    for key, kind in required.items():
        _ensure(key in row, f"missing field: {key}")
        _ensure(row[key] is not None, f"null field: {key}")
        _ensure(isinstance(row[key], kind), f"wrong type for {key}")


def validate_direct_result(row: dict[str, Any], label_field: str) -> None:
    _validate_required(row, DIRECT_RESULT_REQUIRED)
    _ensure(label_field in row, f"missing field: {label_field}")
    _ensure(row[label_field] is not None, f"null field: {label_field}")
    _ensure(isinstance(row[label_field], (int, str)), f"wrong type for {label_field}")
    _ensure(row["track"] == "direct", "track must be direct")
    _ensure(row["verdict"] in {"CORRECT", "WRONG"}, "invalid verdict")


def validate_direct_report(payload: dict[str, Any], label_field: str) -> None:
    _ensure(payload.get("track") == "direct", "track must be direct")
    _ensure(isinstance(payload.get("git_commit"), str), "missing git_commit")
    _ensure(isinstance(payload.get("overall"), dict), "missing overall")
    _ensure(isinstance(payload.get("segments"), list), "missing segments")
    _ensure(isinstance(payload.get("question_inventory"), list), "missing question_inventory")
    overall = payload["overall"]
    _ensure(isinstance(overall.get("sections"), list), "overall.sections missing")
    for segment in payload["segments"]:
        _ensure(segment.get("label_type") == label_field, "wrong segment label_type")
        _ensure(isinstance(segment.get("label"), str), "missing segment label")
        _ensure(isinstance(segment.get("question_count"), int), "missing segment question_count")
        _ensure(isinstance(segment.get("questions"), list), "missing segment questions")
        _ensure(isinstance(segment.get("report"), dict), "missing segment report")
    if "vector_runtime" in payload:
        _ensure(isinstance(payload["vector_runtime"], dict), "vector_runtime must be a dict")


def validate_llm_result(row: dict[str, Any], label_field: str) -> None:
    _validate_required(row, LLM_REQUIRED)
    _ensure(label_field in row, f"missing field: {label_field}")
    _ensure(row[label_field] is not None, f"null field: {label_field}")
    _ensure(isinstance(row[label_field], (int, str)), f"wrong type for {label_field}")
    _ensure(row["track"] == "llm", "track must be llm")
    _ensure(row["verdict"] in {"CORRECT", "WRONG"}, "invalid verdict")
    _ensure(len(row["judge_votes"]) == 3, "judge_votes must contain 3 entries")


def label_field_for_dataset(dataset_name: str) -> str:
    return "category" if dataset_name == "locomo" else "subset"


# ---------------------------------------------------------------------------
# Provenance fields — unified benchmark suite (PR1)
# These are optional in legacy result files; required in suite-generated
# artifacts.  validate_provenance() returns a list of errors (empty = valid).
# ---------------------------------------------------------------------------

PROVENANCE_FIELDS: dict[str, Any] = {
    "target_system": str,
    "target_version": str,
    "target_model": str,
    "target_model_hash": str,
    "target_config_hash": str,
    "judge_profile": str,
    "judge_model": str,
    "judge_hash": str,
    "dataset_hash": str,
    "harness_version": str,
    "environment": str,
    "seed": int,
    "pinned": bool,
}

# Token-efficiency metrics added per-row by the unified suite.
TOKEN_EFFICIENCY_FIELDS: dict[str, Any] = {
    "retrieved_tokens": (int, float),
    "assembled_context_tokens": (int, float),
}

_VALID_ENVIRONMENTS = {"container", "native"}
_VALID_JUDGE_PROFILES = {"open70b", "frontier", "small"}


def validate_provenance(payload: dict[str, Any]) -> list[str]:
    """Validate optional provenance fields in a top-level result payload.

    Returns a list of error strings.  An empty list means valid.
    Fields absent from the payload are silently skipped (legacy compat).
    """
    errors: list[str] = []
    for key, kind in PROVENANCE_FIELDS.items():
        if key not in payload:
            continue
        val = payload[key]
        if val is None:
            errors.append(f"null provenance field: {key}")
            continue
        if not isinstance(val, kind):
            errors.append(f"wrong type for {key}: expected {kind.__name__}, got {type(val).__name__}")
    if "environment" in payload and payload["environment"] not in _VALID_ENVIRONMENTS:
        errors.append(f"invalid environment: {payload['environment']!r} (expected {_VALID_ENVIRONMENTS})")
    if "judge_profile" in payload and payload["judge_profile"] not in _VALID_JUDGE_PROFILES:
        errors.append(
            f"unknown judge_profile: {payload['judge_profile']!r} (expected {_VALID_JUDGE_PROFILES})"
        )
    return errors
