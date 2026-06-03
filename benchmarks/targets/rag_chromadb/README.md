# Dense/ChromaDB RAG baseline target

Dense retrieval baseline using ChromaDB with `all-MiniLM-L6-v2` sentence embeddings.

## Requirements

```sh
pip install chromadb sentence-transformers
```

## How it works

1. **Ingest**, conversation events are embedded and stored in an ephemeral
   ChromaDB collection using cosine similarity.
2. **Answer**, the question is embedded and the top-20 nearest neighbors are
   retrieved; those documents are assembled into a context block and passed to
   the judge via `AimeeHarness`.

The embedding model (`all-MiniLM-L6-v2`) is pinned in `pin.toml`. Changing the
model invalidates any prior result artifacts.

## Running

```sh
# via the unified runner
python benchmarks/suite/runner.py \
  --target rag_chromadb \
  --dataset locomo \
  --limit 50

# directly (stdio JSON protocol)
echo '{"op":"describe"}' | python benchmarks/targets/rag_chromadb/adapter.py
```

## Pinning

After the first canonical run, record `chromadb` and `sentence-transformers`
versions in `pin.toml` under `runtime.package_version` and commit the result
artifact.
