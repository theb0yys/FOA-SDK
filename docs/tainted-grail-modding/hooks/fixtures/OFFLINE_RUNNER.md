# Semantic Fixture Offline Runner

The project-owned runner is `scripts/tainted_grail/semantic_fixture_runner.py`.

It validates the four Batch 004 JSON manifests as inert specifications. It uses only the Python standard library and performs no network access, assembly loading, reflection, game discovery, or evidence promotion.

## Run

```text
python scripts/tainted_grail/semantic_fixture_runner.py \
  docs/tainted-grail-modding/hooks/fixtures \
  --pretty
```

Write a deterministic receipt:

```text
python scripts/tainted_grail/semantic_fixture_runner.py \
  docs/tainted-grail-modding/hooks/fixtures/batch-004-economy-profile.json \
  docs/tainted-grail-modding/hooks/fixtures/batch-004-diagnostic-writer.json \
  docs/tainted-grail-modding/hooks/fixtures/batch-004-wolf-mount-rollback.json \
  docs/tainted-grail-modding/hooks/fixtures/batch-004-avalon-companions-api.json \
  --output build/semantic-hook-fixtures/receipt.json \
  --pretty
```

The runner checks schema version, `specification-only` state, profile identity, Mono binding, absent runtime fingerprints, exact source/blob formatting, unique source paths and case IDs, required expectations, and a decision granting no promotion.

Receipts contain manifest SHA-256 values, counts, structural errors, `runtime_authority: none`, and `promotion: none`.

## Exit codes

- `0`: all manifests are valid specifications;
- `1`: one or more manifests are invalid;
- `2`: usage, discovery, or receipt I/O failure.

## Tests

```text
cd scripts/tainted_grail
python -m unittest -v test_semantic_fixture_runner.py
```

A valid receipt proves only conformance to the offline specification contract. It does not prove target existence, assembly identity, safe mutation, rollback, support-safe diagnostics, combined-mod compatibility, or promotion readiness.
