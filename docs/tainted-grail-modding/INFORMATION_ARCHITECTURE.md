# Modding Handbook Information Architecture

## Purpose

The handbook must be useful to a first-time mod author without weakening FOA-SDK's evidence, governance, legal, and authority boundaries. It therefore separates explanation from canonical data and separates stable knowledge from version-bound observations.

## Content classes

| Class | Purpose | Canonical owner | Handbook treatment |
|---|---|---|---|
| Tutorial | Complete a bounded task | `docs/tainted-grail-modding/` | Write as an ordered, testable procedure |
| How-to guide | Solve a specific problem | `docs/tainted-grail-modding/` | State prerequisites, profile, result, and rollback |
| Concept | Explain architecture or game/modding behavior | handbook or SDK docs | Link to contracts; do not reproduce schemas |
| System reference | Describe entities, relationships, lifecycle, and extension surfaces | catalog, Research, system-port track | Present reviewed summaries and exact links |
| Hook record | Describe one exact extension or patch target | `hooks/` plus evidence owner | Use the hook record contract and exact profile |
| Version profile | Bind game, branch, runtime, loader, framework, and evidence | adapter/catalog records | Never merge Mono and IL2CPP profiles |
| Example | Demonstrate a reviewed pattern | project-owned example source | Keep small, buildable, and separately licensed |
| Process | Govern research, design, review, validation, release, and support | governance and handbook | Adapt the proven source process to current SDK gates |
| Research brief | Define an unresolved question and evidence needed | `Research/` | Link from the backlog; do not present as fact |
| Canonical data | Schemas, records, IDs, relationships, evidence, fingerprints | Gems, Plugins, Research registers | Link or render read-only; never hand-maintain a duplicate |

## Navigation model

The handbook has five primary routes:

1. **Learn** — environment, first mod, runtime concepts, first build.
2. **Build** — research, design, implementation, review, validation, package, release.
3. **Understand** — game systems, data structures, lifecycle, identities, relationships.
4. **Extend** — hook records, SDK extension points, providers, runtime adapters.
5. **Diagnose** — logs, compatibility, conflicts, support, migrations, known failures.

Every detailed page must be reachable from one primary route and may be cross-linked from others.

## Required page metadata

Technical pages must state:

```yaml
status: planned | draft | reviewed | validated | deprecated
page_kind: tutorial | how-to | concept | system-reference | hook-record | process | reference
source_baseline:
  repository: owner/name
  commit: 40-character SHA
version_scope:
  game_version: exact | range | unknown
  branch: exact | unknown
  runtime: Mono | IL2CPP | editor-only | engine-neutral | unknown
  loader: exact version | not-applicable | unknown
evidence_state: raw | candidate | reviewed | runtime-validated | blocked
owner: handbook | research | catalog | plugin | installer | sdk-core
last_reviewed: YYYY-MM-DD
limitations:
  - explicit limitation
```

A page may use prose front matter initially, but these fields must remain explicit and machine-extractable before the handbook is declared complete.

## Single-source rules

- Stable project IDs and exact native references belong in canonical catalog records.
- Evidence and source provenance belong in source/evidence registers.
- Schemas belong with their owning implementation.
- Product behavior belongs in `docs/tainted-grail-sdk/`.
- Handbook pages explain how to use and reason about those records.
- Cross-domain scopes are indexes, not copied subject records.
- Examples may copy only project-owned or explicitly redistributable code.

## System-page shape

Each game-system page should use the same structure:

1. purpose and player-visible behavior;
2. canonical entities and relationships;
3. lifecycle and state transitions;
4. relevant assemblies/types only when permitted and verified;
5. known authoring surfaces in FOA-SDK;
6. known runtime routes;
7. hook records;
8. compatibility and collision risks;
9. evidence and validation state;
10. unresolved research;
11. explicit prohibitions and non-goals.

## Tutorial acceptance

A tutorial is not complete until:

- prerequisites are explicit;
- commands and paths are parameterized rather than tied to a private machine;
- expected output and failure states are described;
- validation proves the intended result;
- cleanup or rollback is documented;
- version scope is exact;
- prohibited payloads are absent;
- all linked product behavior exists at the referenced SDK commit.

## Deprecation and staleness

Pages are not silently rewritten when a patch invalidates them. The old profile remains identifiable, the replacement profile links back to it, and affected hook/system records are marked stale, superseded, or blocked with the evidence that caused the transition.