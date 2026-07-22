# Getting Started

This lane provides the shortest safe path from no local setup to a validated, removable first mod.

## Read order

1. Read the exact pinned observations and route rules in [`../runtime/VERIFIED_PROFILES.md`](../runtime/VERIFIED_PROFILES.md).
2. Complete the [first-mod path](FIRST_MOD_PATH.md).
3. Use the [mod-development process](../process/README.md) for research, design, review, evidence, packaging and support.
4. Add gameplay changes only through a promoted record from the [hook catalogue](../hooks/README.md).

## Complete first-mod path

The current first-mod unit covers:

1. selecting and recording an exact game/runtime/loader profile;
2. keeping game, Unity and BepInEx dependencies local;
3. creating unique assembly, namespace and plug-in identity;
4. implementing a minimal load/log/config lifecycle without a gameplay patch;
5. rebuilding after every identity, project, dependency or source change;
6. controlled local deployment;
7. loader and plug-in startup evidence;
8. harmless configuration round-trip evidence;
9. deterministic removal and cleanup;
10. mod record, compatibility and provenance documentation;
11. project-owned package contents;
12. hook promotion requirements before gameplay mutation.

## Remaining pages

- project-owned downloadable starter source;
- route-specific local-reference examples;
- automated package-layout validation;
- controlled deployment/removal tooling;
- first validation-receipt schema and example;
- focused troubleshooting decision tree;
- IL2CPP executable tutorial after that route is independently qualified.

## Publication gate

The process contract and pinned profiles are documented, but the tutorial is not represented as a clean-machine executable acceptance result. A first-mod tutorial may be marked fully reviewed only after it has been completed from a clean workspace against the exact documented game, branch, runtime, loader, framework and SDK profile.

A contract-only or inert adapter route is described as such and does not receive an executable tutorial.