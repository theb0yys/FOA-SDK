# Installer suite definitions

Each reviewed suite occupies one directory:

```text
Installer/Suites/<Suite>/suite.json
```

`<Suite>` must use a stable portable directory name and `suite.json` must conform to `../suite.schema.json`.

A suite selects package IDs and user-facing features. It does not duplicate package payloads, weaken package compatibility, grant authority, or override licence and redistribution blockers. Suite resolution must be deterministic and reject duplicate IDs, unknown packages, dependency cycles, conflicts, unsupported hosts, and ambiguous ordering.

## Current reviewed catalogue

`DeveloperPreview/suite.json` is the first production catalogue consumed by the resolver and future Suite Wizard. It requires the pinned SDK foundation descriptor, selects the pinned Editor project descriptor by default, and exposes the installer-suite documentation as an optional feature.

The suite is deliberately non-executable and descriptor-only. It does not claim to contain the complete compiled SDK or Editor payload, acquire sources, copy files, install software, elevate privileges, launch the game, deploy mods, sign output, or publish releases.
