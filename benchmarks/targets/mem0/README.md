# mem0 baseline target

mem0 memory storage and retrieval baseline. Uses the `mem0ai` Python package.

## Requirements

```sh
pip install mem0ai
```

## How it works

1. **Ingest**, each conversation event is added to a mem0 `Memory` instance
   via `mem.add(content, user_id=session_id)`.
2. **Answer**, the question is passed to `mem.search(question, user_id=..., limit=20)`;
   the returned memories are assembled into a context block and passed to the judge
   via `AimeeHarness`.

## Running

```sh
# via the unified runner
python benchmarks/suite/runner.py \
  --target mem0 \
  --dataset locomo \
  --limit 50

# directly (stdio JSON protocol)
echo '{"op":"describe"}' | python benchmarks/targets/mem0/adapter.py
```

## Pinning

After the first canonical run, record the exact `mem0ai` version in `pin.toml`
under `runtime.package_version` and commit the result artifact. Future runs
that use a different version are flagged by the runner.
