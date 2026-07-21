# Tainted Framework Editor Services Review Gate

Status: source implementation and focused mutation validation complete; exact-head O3DE host validation pending.

## Implemented

- persistent `FoundationService` ownership;
- safe binding through sanitized `ExtensionAPI::ProfileView` copies;
- exact game, branch, runtime, and BepInEx compatibility projection;
- order-independent, fail-closed compatibility conflict handling;
- stable API-surface decisions with blocked/disabled binding refusal;
- exact default-safe configuration metadata bound to canonical golden fixtures;
- sorted diagnostic vocabulary bound to canonical golden fixtures;
- deterministic, non-executable activation plans;
- governed candidate-evidence eligibility requiring exact readiness and a consumer-ready surface;
- production-linked compiled tests;
- focused mutation validator and local-validation registration;
- public service documentation.

## Required before accepted

1. Run `python Gems/TaintedGrailModdingSDK/Tools/run_local_validation.py --keep-going`.
2. Run `git diff --check <reviewed-main> HEAD`.
3. Configure the approved O3DE host build.
4. Build `Editor` and `TaintedGrailModdingSDK.Catalog.Tests`.
5. Run the production-linked Catalog CTest target.
6. Review the exact-head source for dependency and authority drift.
7. Record the exact commit and resulting local/host evidence.

## Boundary

This unit does not introduce an Editor pane and therefore does not change the current Windows pane count. It does not execute or link BepInEx, Unity, Harmony, Mono-host, IL2CPP-host, process, filesystem, deployment, signing, publication, or save behavior.

The acquisition-provider layer remains hard-gated behind this exact-head acceptance. Its next scope is local filesystem, pinned GitHub, and optional Merlin acquisition with exact provenance and governed candidate-evidence return. Road Atlas, Avalon AI, and runtime adapters remain later units.
