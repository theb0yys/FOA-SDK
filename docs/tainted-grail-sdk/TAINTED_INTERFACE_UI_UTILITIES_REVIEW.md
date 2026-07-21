# Tainted Interface UI Utilities Review Gate

The unit is review-ready only when all statements below are true for one exact commit.

## Provenance

- Tainted Interface repository, branch, commit, version and plugin GUID remain pinned.
- All seven selected source bindings retain their exact Git blob SHA-1 values.
- The upstream licence remains `NOASSERTION` until separately established.
- No upstream PNG or full source-library payload is present in the PR.

## Contract

- The catalog contains exactly the nineteen accepted semantic IDs.
- Catalog output, style tokens, metric tokens and source bindings are deterministic copies.
- Unknown, malformed, uppercase, traversal-like and path-like IDs fail closed.
- Upstream payload and redistribution flags remain false for every catalog entry.
- Asset resolution clearly separates project-owned fallback availability from blocked upstream payloads.

## Embedded assets

- Exactly three project-owned SVG fallback classes exist.
- Each SVG remains bounded and its SHA-256 is recomputed by the validator.
- Fallback provenance says project-owned and not upstream visual parity.
- No arbitrary file path, resource name, byte payload or full-library enumeration API is exposed.

## Authority

- `FoundationService` owns one persistent utility service.
- No BepInEx, UnityEngine, Harmony, Qt file/image class, filesystem, process or runtime host dependency enters the Framework source.
- No mutable catalog, source registry, workspace or profile reference is exposed.
- No file write, deployment, launch, save, publication or runtime mutation is introduced.

## Tests and gates

- `TaintedInterfaceUiUtilitiesTests.cpp` is compiled through the production-linked catalog test target.
- The focused validator and its mutation tests pass.
- `run_local_validation.py --keep-going` passes in a complete checkout.
- Reviewed-range `git diff --check` passes.
- O3DE configure/build and `TaintedGrailModdingSDK.Catalog.Tests` CTest pass.

## Honest status

This unit adds no Editor pane and no Windows pane-count change. It is not visual-parity evidence, upstream asset redistribution approval, an acquisition provider, or a Mono/IL2CPP runtime adapter.
