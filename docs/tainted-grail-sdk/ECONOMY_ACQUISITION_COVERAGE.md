# Economy Acquisition Coverage Dashboard

## Status

Implemented as Correction Slice 6. The feature is a Read-only research and authoring-quality view over the existing canonical catalog, source/evidence registry, governance state, and open blockers.

## Purpose

The **Tainted Grail Economy Acquisition Coverage** pane answers a narrow question: for each canonical economy item or recipe, which known acquisition relationships are sufficiently supported to be treated as covered research, and which lanes remain partial, blocked, or missing?

It does not create records, relationships, evidence, validation events, governance decisions, permissions, runtime commands, or durable schema fields.

## Build ownership

- `EconomyCoverageService` belongs to `TaintedGrailModdingSDK.Core.Static`.
- `EconomyCoverageDashboardWidget` belongs to the Editor target.
- The Core service has no Qt, AzToolsFramework, persistence, Foundation orchestration, or runtime dependency.
- The Editor pane consumes a snapshot of existing state and exposes no mutating control.

## Coverage lanes

Canonical `economy/item` records receive:

- `vendor` from `sold_by` relationships;
- `loot` from `dropped_by` and `found_at` relationships;
- `reward` from `rewarded_by` and `granted_by` relationships.

Canonical `economy/recipe` records receive:

- `reward` from `rewarded_by` and `granted_by` relationships;
- `learnability` from `learned_from` relationships;
- `crafting` from `crafted_at` relationships.

Relationships in other directions or with other kinds do not count toward these lanes.

## Status contract

Each lane reports one of four states:

- `covered` — every matching relationship evaluated for the lane is validated, current, resolved, blocker-free, and backed by usable evidence;
- `partial` — at least one relationship exists, but the relationship, source, target, or evidence is not yet complete enough for covered research;
- `blocked` — the source, a matching relationship, its target, its evidence binding, or an applicable open blocker fails closed;
- `missing` — no matching relationship exists and the source subject itself is not blocked.

A row is `covered` only when every applicable lane is covered. Any blocked lane makes the row blocked. Otherwise, a mixture of covered, partial, and missing lanes is partial; a row with only missing lanes is missing.

A covered status is not a permission grant and is not runtime proof.

## Evidence and blocker handling

Evidence is usable only when:

- the evidence ID resolves in the source/evidence registry;
- its source exists;
- fingerprint, profile ID, game version, and branch match the source record;
- its subject matches the source economy subject or the acquisition target subject.

Unknown evidence, missing sources, mismatched bindings, or unrelated subjects block the affected lane. Unresolved targets remain partial unless another failure makes them blocked.

Open blockers attached to the source or acquisition target are surfaced by ID and reason. Closed blockers do not affect coverage.

## Determinism and non-mutation

Rows are sorted by canonical record ID. Relationship, evidence, blocker, and reason lists are sorted and deduplicated. Analysis accepts immutable catalog, registry, and blocker inputs and does not modify them.

Focused tests cover:

- deterministic item vendor, loot, and reward separation;
- recipe learnability, crafting, and reward separation;
- fail-closed unrelated evidence and open blockers;
- catalog and registry non-mutation.

## Editor presentation

The pane displays:

- canonical record ID and display name;
- record kind and owner pack;
- vendor, loot, reward, learnability, and crafting lane states;
- overall status plus validation and staleness;
- relationship IDs, evidence IDs, blocker IDs, and reasons.

The table is non-editable and refreshes when Foundation state changes.

## Runtime boundary

No runtime adapter is added. The feature does not launch FoA, deploy files, inject code, mutate vendors or loot, grant rewards, learn recipes, edit saves, or produce adapter work orders.

## Next slice

The next ordered economy step is authoring-time duplicate detection across packs. It remains outside this dashboard change.
