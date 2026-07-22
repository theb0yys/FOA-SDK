# Batch 004 Exact-Profile Fixture Manifests

These files are deterministic, machine-readable fixture **specifications** for Semantic Hook Batch 004. They are not execution receipts and do not contain game assemblies, loader binaries, compiled plug-ins, saves, logs, private paths, or runtime captures.

## Exact profile binding

Every manifest is bound to:

```text
profile_id: foa-1.23.401-mono-bepinex5-tf-0.1.33
game: 1.23.401
unity: 6000.0.64f1
runtime: Mono
loader: BepInEx 5.4.23.3
framework: Tainted Framework 0.1.33
profile evidence state: HostLiveLoadValidated
```

That profile proves a pinned Mono host/load route. It does **not** by itself prove any selected `TG.Main`, Harmony, optional-mod, reflection, mutation, diagnostics, cleanup, or combined-mod behavior.

Each manifest therefore requires exact runtime assembly fingerprints and records them as `required-but-absent`. A fixture result is inadmissible when the profile ID, source commit, source blobs, assembly fingerprints, test implementation identity, or observation digest is missing or mismatched.

## Fixture contract

Each JSON manifest contains:

- `fixture_set_id` — stable Batch 004 fixture identity;
- exact upstream repository, commit, paths, and source blobs;
- exact handbook profile identity;
- `execution_state` — currently `specification-only`;
- an explicit fail-closed acceptance rule;
- deterministic cases with setup, expected result, and promotion effect;
- a final promotion or prohibition decision.

The manifests do not provide a runner. A future runner must:

1. operate only on operator-supplied, locally fingerprinted assemblies;
2. keep Mono and IL2CPP fixtures separate;
3. use synthetic or approved disposable state;
4. inject deterministic failures where rollback is claimed;
5. emit bounded, redacted, content-addressed observations;
6. perform no deployment, save mutation, publication, or evidence promotion;
7. return candidate evidence for independent review.

## Fixture sets

- [`batch-004-economy-profile.json`](batch-004-economy-profile.json) — reflection, quantity, reward-Wealth, vendor-price, and regional-price cases.
- [`batch-004-diagnostic-writer.json`](batch-004-diagnostic-writer.json) — path containment, traversal, row/byte bounds, atomicity, redaction, retention, and CSV safety.
- [`batch-004-wolf-mount-rollback.json`](batch-004-wolf-mount-rollback.json) — ownership, created object graph, private fields, movement suspension, native actions, scene transitions, and rollback.
- [`batch-004-avalon-companions-api.json`](batch-004-avalon-companions-api.json) — API v1 source contract, compiled assembly identity, owner isolation, duplicates, gate races, load order, and cleanup.

## Evidence ceiling

Until a reviewed runner executes a manifest against all exact required fingerprints, the fixture remains `specification-only`. Passing synthetic cases alone cannot promote a runtime hook. Adverse source behavior may, however, justify an explicit current-source prohibition without executing the game.
