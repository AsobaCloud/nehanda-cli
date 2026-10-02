#!/usr/bin/env python3
"""JSON-lines stdio worker for Laya System-1 decisions.

Loads the real convaiinnovations/laya checkpoint on `load` / first predict.
Never silently invents answers — if Laya cannot load, every request returns ok:false.

Protocol (one JSON object per line):
  {"id":1,"method":"ping"}
  {"id":2,"method":"load"}           # download + load checkpoint into memory
  {"id":3,"method":"predict","params":{"state":"...","questions":{...}}}
  {"id":4,"method":"shutdown"}
"""
from __future__ import annotations

import json
import sys
import traceback
import time

_router = None
_agent = None
_loaded = False
_load_error = None


def _ensure_loaded():
    global _router, _agent, _loaded, _load_error
    if _loaded:
        return
    if _load_error is not None:
        raise RuntimeError(_load_error)
    t0 = time.time()
    try:
        from laya import Router
        _router = Router()
        # Force English checkpoint into memory so warm predict hits ≤50ms path.
        # Router downloads on first predict; prime with a tiny call.
        _router.predict(
            "nehanda system-1 warm start",
            {
                "ready": {
                    "type": "noul",
                    "instructions": "Is the control plane ready?",
                }
            },
            model="english",
        )
        _loaded = True
        ms = int((time.time() - t0) * 1000)
        print(f"[laya-worker] loaded english checkpoint in {ms}ms", file=sys.stderr)
    except TypeError:
        # Older Router.predict may not accept model= kwarg
        try:
            from laya import Router
            _router = Router()
            _router.predict(
                "nehanda system-1 warm start",
                {"ready": {"type": "noul", "instructions": "Is the control plane ready?"}},
            )
            _loaded = True
            ms = int((time.time() - t0) * 1000)
            print(f"[laya-worker] loaded via Router in {ms}ms", file=sys.stderr)
        except Exception:
            try:
                import laya
                _agent = laya.load("convaiinnovations/laya")
                _agent.predict(
                    "nehanda system-1 warm start",
                    {"ready": {"type": "noul", "instructions": "Is the control plane ready?"}},
                )
                _loaded = True
                ms = int((time.time() - t0) * 1000)
                print(f"[laya-worker] loaded laya.load() in {ms}ms", file=sys.stderr)
            except Exception as e2:
                _load_error = f"laya load failed: {e2}"
                raise RuntimeError(_load_error) from e2
    except Exception as e:
        _load_error = f"laya load failed: {e}"
        raise RuntimeError(_load_error) from e


def predict(state, questions):
    _ensure_loaded()
    if _router is not None:
        try:
            return _router.predict(state, questions, model="english")
        except TypeError:
            return _router.predict(state, questions)
    return _agent.predict(state, questions)


def _to_dict(result):
    if result is None:
        return {}
    if isinstance(result, dict):
        # answers may contain nested objects
        out = {}
        for k, v in result.items():
            if hasattr(v, "keys") and not isinstance(v, dict):
                try:
                    out[k] = dict(v)
                    continue
                except Exception:
                    pass
            if isinstance(v, dict):
                nested = {}
                for nk, nv in v.items():
                    if hasattr(nv, "keys") and not isinstance(nv, dict):
                        try:
                            nested[nk] = dict(nv)
                            continue
                        except Exception:
                            pass
                    nested[nk] = nv
                out[k] = nested
            else:
                out[k] = v
        return out
    if hasattr(result, "keys"):
        return dict(result)
    return {"raw": str(result)}


def handle(msg):
    method = msg.get("method")
    mid = msg.get("id")
    params = msg.get("params") or {}
    if method == "ping":
        return {"id": mid, "ok": True, "result": {"pong": True, "loaded": _loaded}}
    if method == "load":
        _ensure_loaded()
        return {"id": mid, "ok": True, "result": {"loaded": True}}
    if method == "shutdown":
        return {"id": mid, "ok": True, "result": {"bye": True}, "_exit": True}
    if method == "predict":
        result = predict(params.get("state", ""), params.get("questions") or {})
        return {"id": mid, "ok": True, "result": _to_dict(result)}
    return {"id": mid, "ok": False, "error": f"unknown method: {method}"}


def main():
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            msg = json.loads(line)
        except json.JSONDecodeError as e:
            sys.stdout.write(json.dumps({"ok": False, "error": f"bad json: {e}"}) + "\n")
            sys.stdout.flush()
            continue
        try:
            resp = handle(msg)
            do_exit = resp.pop("_exit", False)
            sys.stdout.write(json.dumps(resp, default=str) + "\n")
            sys.stdout.flush()
            if do_exit:
                return 0
        except Exception as e:
            traceback.print_exc(file=sys.stderr)
            sys.stdout.write(json.dumps({
                "id": msg.get("id"),
                "ok": False,
                "error": str(e),
            }) + "\n")
            sys.stdout.flush()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
