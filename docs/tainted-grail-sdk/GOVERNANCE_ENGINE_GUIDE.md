# Catalog Governance Engine Guide

## Purpose

The catalog governance engine records reviewed decisions about what a canonical record or relationship means, how trustworthy it is, whether it is current, and what named usages are permitted or prohibited.

Open it from:

**Tools → Tainted Grail SDK → Tainted Grail Catalog Governance**

The engine does not execute gameplay, deploy files, patch FoA, or grant blanket runtime authority. It produces durable editor-side governance state and blockers for downstream planning and adapter review.

## Core rule

The following axes are independent:

- maturity/research stage;
- confidence;
- operational risk;
- validation state;
- staleness;
- allowed usages;
- forbidden usages;
- supersession.

A stronger value on one axis does not silently change another axis.

**Validation does not grant permission.**

A catalog subject can be validated but still have no allowed usage. An allowed usage requires its own reviewed permission decision backed by validation proof for the same subject.

## Supported subjects

Governance applies to:

- canonical records;
- canonical relationships.

Relationships use the same independent maturity, confidence, risk, validation, staleness, permission, prohibition, and supersession concepts as records.

## Prerequisites

Before making a governance decision:

1. open a saved workspace;
2. configure an exact active game profile;
3. load the canonical catalog;
4. select a record or relationship;
5. identify supporting evidence;
6. name the reviewer or validator;
7. use a separate validation decision before granting an allowed usage.

Every decision is persisted inside:

```text
Catalog/catalog.tgcatalog.json
```

The in-memory catalog changes only after the complete document saves successfully.

## Maturity/research stage

The primary maturity vocabulary is:

- `S0` — untriaged or merely discovered;
- `S1` — source located;
- `S2` — evidence extracted;
- `S3` — identity partially established;
- `S4` — relationships partially mapped;
- `S5` — reviewed record;
- `S6` — reconciled across relevant sources;
- `S7` — validated for one or more defined purposes;
- `S8` — release-authoring ready under explicit permissions.

Legacy descriptive values remain readable for backward compatibility, but new work should prefer `S0` through `S8`.

Changing maturity requires:

- an existing subject;
- evidence IDs;
- a reviewer;
- optional notes explaining the review.

Maturity is not permission.

## Confidence

Supported confidence values are:

- `unknown`;
- `hypothesis`;
- `inferred`;
- `documented`;
- `runtime_observed`;
- `validated`.

Confidence describes support for a claim. It does not describe operational safety.

For example, a runtime-observed quest mutation path may have high factual confidence while still carrying critical operational risk and remaining forbidden for use.

## Operational risk

Supported values are:

- `unknown`;
- `low`;
- `medium`;
- `high`;
- `critical`.

Risk should consider:

- save compatibility;
- persistence and cleanup;
- uniqueness and duplication;
- quest or world-state effects;
- asset and memory lifetime;
- failure recovery;
- game-version sensitivity;
- downstream adapter capability.

Risk does not grant or deny usage automatically. It informs the explicit permission decision and remains visible as its own axis.

## Validation decisions

Supported states are:

- `unvalidated`;
- `pending`;
- `validated`;
- `failed`;
- `stale`;
- `blocked`.

A validation decision requires:

- subject kind and ID;
- state;
- validation method or test protocol;
- evidence IDs;
- named validator;
- optional notes.

The validation event records the active profile ID, exact game version, and branch.

### Fail-closed effects

When validation is not `validated`, existing allowed usages are removed.

The engine adds prohibitions such as:

- `no_unvalidated_runtime_use`;
- `validation_failed`;
- `stale_or_unverified`.

A successful validation removes the automatic unvalidated/failure flags, but it does not add an allowed usage.

## Staleness

Supported states are:

- `unknown`;
- `current`;
- `potentially_stale`;
- `stale`.

Promoted catalog records begin with `unknown` staleness. A reviewer must explicitly assess the record against the active exact game build.

Changing a subject to `potentially_stale` or `stale`:

- removes all allowed usages;
- adds `stale_or_unverified`;
- creates a durable governance event;
- surfaces blockers in the Foundation Status window.

Returning a subject to `current` removes the automatic stale prohibition, but does not restore prior permissions. Usage must be reviewed again.

## Permissions and prohibitions

Permissions are named usage lanes, not a global safe/unsafe switch.

Examples:

- `display_only`;
- `planning_only`;
- `catalog_reference`;
- `grant_existing_item`;
- `learn_existing_recipe`;
- `spawn_static`;
- `spawn_patrol`;
- `attach_route`;
- `quest_read`;
- `quest_mutation`.

The actual vocabulary will expand with adapter contracts. Until a lane is defined and reviewed, it should remain unset or forbidden.

### Allowing a usage

An `allow` decision requires:

- validated subject state;
- `current` staleness state;
- no missing references;
- no unresolved conflicts;
- no supersession;
- evidence IDs;
- one or more validation event IDs;
- at least one referenced validation event with state `validated`;
- a named reviewer.

The validation proof must belong to the same record or relationship.

### Forbidding a usage

A `forbid` decision:

- removes that usage from the allowed list;
- adds it to the prohibited list;
- records evidence, reviewer, time, and notes.

Prohibitions are first-class decisions. They are not merely missing permissions.

### Clearing a usage decision

A `clear` decision removes the named usage from both allowed and forbidden lists. Clearing does not imply permission.

## Supersession

Supersession replaces a record with another existing record, or a relationship with another existing relationship.

A supersession decision requires:

- a different existing replacement ID;
- evidence;
- a reviewer.

The superseded subject becomes stale, loses all allowed usages, and gains the `superseded` prohibition.

Supersession history is durable and the original subject remains inspectable.

## Governance history

Every reviewed decision records:

- stable event ID;
- subject kind and ID;
- axis;
- previous value;
- new value or permission outcome;
- named usage when relevant;
- evidence IDs;
- validation proof IDs;
- reviewer;
- decision timestamp;
- notes.

History is append-only through the editor workflow. Do not edit prior events to rewrite a decision. Record a new decision that changes the current state.

## Blockers

The governance blocker service reports:

- unknown maturity, confidence, risk, or staleness;
- stale or potentially stale subjects;
- allowed usages without a current reviewed permission event;
- allowed usages without validated proof;
- permissions retained by superseded subjects;
- governance history referencing missing evidence or validation events;
- validation proof bound to another profile, game version, or branch.

Warnings describe incomplete review. Errors block affected usage lanes.

## Relationship governance

A relationship can be factually supported while one or both connected records remain incomplete. Relationship evidence must already be linked to that relationship before governance or validation decisions use it.

The same rules apply:

- validation does not grant permission;
- stale relationships lose allowed usage;
- unresolved targets or conflicts block permission;
- superseded relationships remain in history but cannot remain allowed.

## What the engine does not do

It does not:

- infer maturity from record count;
- copy confidence from evidence automatically after promotion;
- convert validation into permission;
- restore permissions after staleness is cleared;
- resolve conflicts automatically;
- choose a superseding identity automatically;
- create runtime work orders;
- invoke adapters;
- modify FoA.

## Review workflow

For a typical subject:

1. promote evidence into a reviewed canonical record;
2. assess maturity;
3. assess confidence;
4. assess operational risk;
5. assess staleness against the exact active build;
6. run and record a validation method;
7. review one named usage lane;
8. allow or forbid that lane explicitly;
9. inspect blockers and history;
10. repeat for other usage lanes independently.

A subject may therefore be safe to display, safe to plan with, blocked for spawning, and forbidden for quest mutation at the same time.
