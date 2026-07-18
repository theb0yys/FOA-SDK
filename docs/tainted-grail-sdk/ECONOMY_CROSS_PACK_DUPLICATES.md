# Economy Cross-Pack Duplicate Report

## Status

Implemented as Correction Slice 7. The feature is a **Read-only** authoring-time report over the existing canonical catalog, typed economy profiles, source/evidence registry, governance state, and open blockers.

## Purpose

The **Tainted Grail Economy Cross-Pack Duplicates** pane identifies exact duplicate candidates declared by distinct owner packs before adapter work orders or runtime integration exist.

It does not decide that two records are semantically identical. It does not merge records, reject a pack, select a winning record, grant permission, or prove a runtime conflict.

## Exact detection signals

A group is emitted only when at least two distinct owner packs share one of these exact, case-sensitive signals:

- the same economy record kind and exact `subjectRef`;
- for recipes, the same exact recipe `duplicateKey`.

The signals remain separate. The same candidate records may appear in both groups when both exact conditions hold.

Display names, aliases, localisation text, tags, asset paths, spelling similarity, case folding, fuzzy matching, and inferred semantics are never duplicate identity signals.

## Candidate health

Every candidate includes its canonical record ID, owner pack, exact subject, recipe duplicate key when applicable, validation and staleness state, evidence IDs, open blocker IDs, and reasons.

The Core service validates evidence before reporting a healthy candidate:

- evidence IDs must resolve;
- the source must resolve;
- source fingerprint, profile ID, game version, and branch must match the evidence binding;
- evidence must belong to the candidate record's exact subject;
- item or recipe profile evidence is included when the typed profile exists.

A missing typed economy profile makes the candidate partial. Unknown or unrelated evidence, source-binding drift, stale or failed state, missing references, conflicts, supersession, and open blockers fail closed.

## Group states

- `review_required` — every candidate is validated, current, evidence-backed, reference-complete, conflict-free, non-superseded, and free of open blockers. Human review is still required.
- `partial` — the exact cross-pack signal exists, but at least one candidate is not yet validated/current or lacks its typed economy profile.
- `blocked` — at least one candidate has invalid evidence, a hard governance/reference failure, supersession, or an open blocker.

No state means "confirmed duplicate" or "safe to delete."

## Determinism and immutability

- only pack-owned canonical economy items and recipes are scanned;
- same-pack repeats do not create a cross-pack group;
- keys are exact and case-sensitive;
- groups sort by signal, record kind, and exact key;
- candidates sort by owner pack and canonical record ID;
- input catalog, profile, source/evidence, governance, and blocker state is never mutated.

## Build ownership

- `EconomyDuplicateDetectionService` belongs to `TaintedGrailModdingSDK.Core.Static`;
- `EconomyDuplicateReportWidget` belongs to the Editor target;
- tests link Framework and receive Core transitively;
- the focused validator enforces exact matching, distinct-pack gating, pure Core dependencies, read-only UI, tests, workflow integration, and documentation.

## Windows duplicate-review companion

`Preview/DuplicateReview/` contains a second project-owned synthetic pack and a two-claim structured source. The Windows manual UI pass copies the companion pack into a freshly generated preview workspace, imports the source, and promotes two item records with the same exact subject under different owner packs. The expected report is one `partial` `subject_ref` group because the promoted candidates deliberately have no typed item profiles.

The companion remains outside the canonical generated persistence fixture so existing fixture hashes and load/save/reopen coverage remain stable.

## Runtime boundary

**No runtime adapter** is added. The report performs no deployment, launch, injection, vendor/loot/reward mutation, recipe learning, registration, persistence mutation, save access, telemetry, or game integration.

## Acceptance

The slice is accepted when:

- exact cross-pack subject matches are reported for items and recipes;
- exact recipe duplicate keys can report different recipe subjects across packs;
- same-pack matches and case-different keys are ignored;
- display-name similarity cannot create a group;
- candidate health produces deterministic `review_required`, `partial`, and `blocked` groups;
- unknown/unrelated evidence and open blockers fail closed;
- inputs remain unchanged;
- the Editor pane contains no editing control;
- the Windows manual UI checklist covers the eighth pane using project-owned synthetic data and the duplicate-review companion.

## Rollback

Revert the implementing pull request. No durable schema or user data requires migration.
