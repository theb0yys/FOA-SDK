# Tainted Framework Editor Services Review Gate

Status: source implementation complete; exact-head host validation pending.

## Implemented

- persistent `FoundationService` ownership;
- exact compatibility projection from the canonical knowledge pack;
- stable API-surface decisions;
- exact default-safe configuration metadata;
- sorted diagnostic vocabulary;
- deterministic, non-executable activation plans;
- governed candidate-evidence eligibility without submission authority;
- production-linked compiled tests;
- focused mutation validator and local-validation registration;
- public service documentation.

## Required before ready for review

1. Run `python Gems/TaintedGrailModdingSDK/Tools/run_local_validation.py --keep-going`.
2. Run `git diff --check <reviewed-main> HEAD`.
3. Configure the approved O3DE host build.
4. Build `Editor` and `TaintedGrailModdingSDK.Catalog.Tests`.
5. Run the production-linked Catalog CTest target.
6. Review the exact-head source for dependency and authority drift.

## Boundary

This unit does not introduce an Editor pane and therefore does not change the current Windows pane count. It does not import UI Framework assets. It does not execute or link BepInEx, Unity, Harmony, Mono-host, IL2CPP-host, process, filesystem, deployment, signing, publication, or save behavior.

The next separately reviewed unit is UI Framework utilities and approved embedded assets.
