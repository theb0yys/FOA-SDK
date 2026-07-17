# Recipe Station Visibility and Learnability Evidence View Design

Status: approved in PR #9; schema-neutral implementation under review

Target: Phase 6 — Domain authoring tools, remaining economy work

## Decision

Implement a schema-neutral, read-only evidence view for canonical recipe–station associations and recipe learnability research.

The implementation derives a deterministic review model from the existing catalog, economy profiles, relationships, evidence registry, governance state, and blockers. It does not add runtime behavior, grant permission, or persist a second representation of recipe visibility or learnability.

## User problem

The Item and Recipe Editor stores and displays:

- canonical recipe profiles;
- canonical station record IDs;
- recipe unlock mode and unlock subject references;
- profile evidence IDs;
- first-class `crafted_at` and `learned_from` relationships;
- read-only action-lane status.

Those facts previously appeared in separate surfaces. A reviewer could not inspect one recipe and answer, per station and learnability source:

- which exact canonical records are involved;
- which evidence supports each association;
- whether evidence is present and subject-compatible;
- whether the relevant records and relationships are current and validated;
- which blockers prevent the research from supporting later adapter planning;
- whether a displayed result is only research evidence rather than runtime proof.

## Product boundary

This change remains inside editor-side knowledge and review tooling.

It must not:

- call FoA, BepInEx, Harmony, or Avalon Core runtime APIs;
- append recipes to station lists;
- learn or forget recipes;
- register custom recipes;
- grant validation or permission;
- generate adapter work orders;
- mutate vendors, loot, rewards, quests, saves, deployment state, or game state;
- infer runtime visibility or learnability from display names or incomplete research.

## Owning architecture layer

The feature belongs to the economy domain read-model layer.

Responsibilities:

- `EconomyAuthoringService` builds recipe–station evidence rows from immutable inputs;
- the service consumes `CatalogDatabase`, `SourceEvidenceRegistry`, and evaluated blockers;
- `ItemRecipeEditorWidget` renders returned rows but does not evaluate domain truth;
- existing catalog, evidence, governance, and blocker services remain authoritative;
- `FoundationNotificationBus` triggers refresh when shared state changes.

The service is deterministic, side-effect free, and safe to call during UI refresh.

## Read-model contents

A row represents one exact recipe–station association or one unresolved station reference. It exposes structured data rather than formatted UI strings as domain truth:

- recipe record ID;
- station record ID or unresolved station subject reference;
- station exact native reference or pack-owner identity where available;
- association sources, including profile station reference and matching `crafted_at` relationship IDs;
- unlock mode;
- exact unlock subject references;
- matching `learned_from` relationship IDs;
- relevant profile and relationship evidence IDs;
- evidence-health status and reasons;
- relevant validation and staleness state;
- current read-only action-lane statuses;
- blocker IDs and actionable blocker reasons.

The read model preserves exact IDs. Display names may be shown as labels only and are never used to join records.

## Evidence semantics

The view reports research support; it does not claim runtime behavior.

### Station association

A station association may be sourced from:

- `EconomyRecipeProfile::m_stationRecordIds`;
- a canonical `crafted_at` relationship from the recipe to the station;
- both sources.

The row shows which source exists rather than silently collapsing them into one inferred fact.

A profile station reference without supporting evidence remains visible as incomplete research. A `crafted_at` relationship with missing, unknown, or incompatible evidence remains visible but blocked.

### Learnability research

Learnability information may be sourced from:

- recipe unlock mode;
- exact unlock subject references;
- canonical `learned_from` relationships;
- evidence attached to the recipe profile and relevant relationships.

The service does not infer that a recipe is learnable merely because an unlock mode or source reference is populated. It reports the available components and their evidence health.

### Evidence compatibility

The implementation uses the existing source/evidence registry and catalog identity rules and fails closed when:

- an evidence ID is unknown;
- evidence does not match its source fingerprint, profile, game version, or branch;
- evidence is unrelated to the recipe, station, learnability target, or explicit unresolved subject;
- a referenced relationship is malformed, stale, conflicted, missing references, or superseded;
- a canonical record is unresolved, conflicted, missing references, stale, or superseded.

Evidence interpretation remains in the service rather than the widget.

## Research status vocabulary

The UI uses neutral research statuses:

- `supported evidence` — the represented association has both profile and relationship sources, resolvable canonical identity, usable evidence, and no applicable governance or blocker failure;
- `partial evidence` — usable evidence exists but only one association source is represented;
- `missing evidence` — the association is represented without usable supporting evidence;
- `unresolved` — a station identity is not reconciled to a canonical record;
- `blocked` — existing blockers, governance state, or invalid evidence prevent the research from supporting downstream planning.

These statuses are not validation states, permission decisions, adapter capability declarations, or runtime test results.

## Fail-closed rules

The view never presents a recipe row as supported when any applicable input is:

- missing;
- unresolved;
- bound to an unrelated subject or inconsistent source profile/build;
- unvalidated, failed, stale, potentially stale, or blocked;
- conflicted;
- missing references;
- superseded;
- covered by an open blocker;
- lacking usable station or learnability evidence.

An `allowed` editor-side action lane may be displayed, but it does not override contradictory evidence or blockers and is not described as runtime capability.

## Persistence and schema

The implementation adds no persisted fields and does not change catalog schema 1.

The read model is rebuilt from existing authoritative state. Opening, filtering, selecting, or refreshing the view does not:

- mutate the catalog;
- append history;
- change validation or permission;
- save the catalog;
- publish a candidate catalog;
- create identities or relationships.

## User interface

The existing Item and Recipe Editor contains a read-only **Station Visibility and Learnability Evidence** group that:

- follows the selected canonical recipe;
- shows one deterministic row per exact station association or unresolved reference;
- exposes association source, evidence health, governance state, blockers, and learnability research;
- provides accessible column labels and actionable detail text;
- contains no editable control for evidence status, validation, permission, or blocker suppression.

No new top-level dock is introduced.

## Validation coverage

Unit and contract coverage includes:

- multiple stations with deterministic ordering;
- a profile station reference combined with a matching `crafted_at` relationship;
- duplicate association suppression without losing source detail;
- matching `learned_from` relationships;
- unresolved station subjects;
- unknown evidence IDs;
- stale station governance;
- read-only action-lane presentation;
- proof that building the view does not mutate records, relationships, or governance history;
- CI contract checks for the service model, UI surface, fail-closed vocabulary, negative tests, and non-mutation assertions.

## Documentation impact

Implementation updates:

- the dedicated Recipe Station Evidence View user guide;
- the documentation hub;
- roadmap status;
- changelog;
- this design record.

`DATA_FORMATS.md` does not change because no persisted field or schema is added.

## Security, privacy, legal, and compatibility impact

- No new filesystem scanning, process execution, networking, telemetry, or deployment.
- No private game data or copyrighted fixtures are committed.
- Existing exact profile/build and evidence provenance rules remain authoritative.
- Existing catalog documents remain readable without migration.
- The feature remains editor-only and host-tool-only.

## Acceptance criteria

- The existing Item and Recipe Editor exposes a usable read-only recipe station and learnability evidence view.
- Every association is built from exact canonical IDs or an explicit unresolved subject reference.
- Association and evidence sources remain visible rather than being silently inferred.
- Missing, mismatched, unresolved, stale, blocked, conflicted, missing-reference, and superseded inputs fail closed with actionable reasons.
- The view cannot grant permission, change validation, suppress blockers, mutate state, or trigger persistence.
- Focused validators and unit tests cover deterministic, negative, duplicate, and non-mutation cases.
- Public documentation distinguishes research evidence from runtime capability.
- Applicable repository validation, source-policy validation, unit tests, and host builds must pass before merge.

## Review boundary

Approval in PR #9 authorised only this schema-neutral read-only implementation. It did not authorise schema changes, adapter contracts, runtime execution, work-order generation, or permission-model changes.
