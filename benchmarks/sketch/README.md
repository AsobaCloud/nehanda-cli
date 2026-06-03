# Sketch Fixture Corpus

Fixtures for `approximate-sketches-for-ingest-prefiltering.md`.

- `exact_duplicates/`: identical payloads for Bloom/content-hash checks.
- `near_duplicates/`: high-overlap text for MinHash/LSH recall checks.
- `distinct_text/`: low-overlap negatives for near-dup false-positive checks.
- `count_min_frequencies/`: token streams with known frequencies.
- `hll_distinct_ids/`: identifier lists with known cardinality.

The committed fixture checks run through the production sketch primitives:

```sh
make -C src build/obj/tests/unit-test-sketch
src/build/obj/tests/unit-test-sketch
```

That test covers exact-duplicate Bloom hits, a 100K-sample Bloom
false-positive bound, MinHash/LSH near-duplicate recall and distinct
negatives, Count-Min frequency error, and HLL distinct-count error.
