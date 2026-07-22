# Installer suite definitions

Each reviewed suite occupies one directory:

```text
Installer/Suites/<Suite>/suite.json
```

`<Suite>` must use a stable portable directory name and `suite.json` must conform to `../suite.schema.json`.

A suite selects package IDs and user-facing features. It does not duplicate package payloads, weaken package compatibility, grant authority, or override licence and redistribution blockers. Suite resolution must be deterministic and reject duplicate IDs, unknown packages, dependency cycles, conflicts, unsupported hosts, and ambiguous ordering.
