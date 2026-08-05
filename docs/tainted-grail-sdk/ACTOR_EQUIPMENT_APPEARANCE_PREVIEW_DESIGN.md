# Actor Equipment and Appearance Preview Design

Status: proposed Stage 8 design for maintainer review. This document defines an editor-only direction; it does not authorise implementation, merge, runtime use, deployment, save interaction, catalog promotion, signing, publication, or redistribution of proprietary content.

Target: Stage 8 of the Visual Game-Content Browser and Preview Pipeline.

## Purpose

The Alpha visual pipeline now has bounded asset discovery, local preview artefacts, neutral handoff, O3DE preview products, Asset Browser evidence, a live product preview path, and explicit Item/Recipe visual selectors. The next ordered visual stage is actor equipment and appearance preview.

The existing Actor and Troop Editor can author evidence-bound actor profiles with separate portrait and model asset references. It can also display catalog relationships. It cannot currently:

- show the actor's portrait or model through the live O3DE previewer path;
- explain whether the stored appearance references still resolve to current, exact-profile preview products;
- present equipment relationships as typed slots;
- preview the product associated with an equipped item;
- distinguish a product-level preview from a reconstructed in-game actor appearance;
- preserve selected preview-product provenance when an appearance reference is changed.

This design defines the smallest reviewable Stage 8 implementation that closes those gaps without inventing Unity runtime facts, skeletal attachment rules, socket transforms, material behavior, animation behavior, inventory semantics, or game-side equip authority.

## Controlling decision

Stage 8 is an **O3DE editor-preview and authoring-evidence feature only**.

It may:

- read existing canonical actor and item records;
- read existing typed actor and item profiles;
- read exact-profile Asset Browser pane models;
- use O3DE's registered Asset Browser previewers for current products;
- update the existing actor portrait or model reference only after an explicit user action;
- write a matching appearance-provenance relationship through the Foundation transaction boundary;
- display reviewed equipment relationships in closed, typed slots;
- preview an equipped item's O3DE product independently from the actor model;
- report explicit fidelity, staleness, validation, evidence, and blocker state.

It may not:

- launch or invoke FoA, Unity, BepInEx, Harmony, Mono, or IL2CPP;
- inspect or modify saves;
- equip an item in the game;
- spawn or animate an actor;
- create a runtime actor, prefab instance, entity, scene object, or attachment socket;
- mutate an O3DE level or Editor viewport;
- promote candidate evidence or create canonical actor/item identities;
- infer an actor or item from a display name;
- grant runtime, save, deployment, catalog-promotion, signing, release, or publication authority;
- commit extracted or derived proprietary game payloads.

## Design decisions requested for approval

### 1. Reuse schema-2 actor and relationship storage

The first Stage 8 implementation does not introduce a catalog schema version.

Appearance references remain in the existing `PopulationActorProfile` fields:

- `PortraitAssetRef`;
- `ModelAssetRef`.

Appearance provenance and equipment assignments use the existing first-class `CatalogRelationship` collection. No widget-owned database, sidecar authoring file, display-name join, or hidden cache becomes authoritative.

### 2. Treat equipment preview as exact reference preview, not reconstructed composition

The first bounded equipment cohort previews:

- the selected actor model product;
- the selected portrait product;
- each equipped item product independently;
- the exact actor-to-item slot relationship and its evidence.

It does **not** attach equipment to a skeleton, bone, socket, mesh, material slot, or runtime actor. The overall equipment fidelity is therefore `partial` unless a later reviewed design supplies exact attachment evidence and a bounded composition contract.

A product preview and an actor-equipment reference table are sufficient for this first Stage 8 slice. They are not sufficient for wording such as "reconstructed actor", "in-game appearance", "equipped character render", or "runtime-compatible loadout".

### 3. Keep equipment relationship authoring outside the first preview slice

The Stage 8 pane consumes equipment relationships read-only. It does not create, delete, supersede, reorder, or move equipment relationships.

This avoids adding undeclared removal semantics and avoids presenting a preview panel as a general catalog relationship editor. Equipment relationships may be supplied by a deterministic synthetic fixture or by an independently reviewed relationship-authoring path.

A later authoring unit may add typed equipment relationship commands only after it defines replacement, clearing, supersession, evidence, rollback, and conflict behavior.

## Pipeline position

The Stage 8 read path is:

```text
FoA native asset reference
→ version-bound discovery record
→ local preview artefact
→ neutral preview handoff
→ generated O3DE preview product
→ Asset Browser pane model
→ live O3DE product preview
→ actor appearance reference / equipment relationship view
```

The actor and item catalog identities remain separate from every preview identity:

```text
canonical actor record
→ typed PopulationActorProfile
→ portrait/model AssetId reference
→ appearance provenance relationship

canonical item record
→ typed EconomyItemProfile
→ item AssetId reference
→ actor equipment relationship
```

No layer silently replaces another.

## Architectural ownership

### Core

Core owns deterministic, engine-neutral rules for:

- accepted Stage 8 equipment relationship kinds;
- relationship-to-slot parsing;
- actor/item record-kind validation;
- typed actor/item profile presence;
- duplicate and conflicting slot detection;
- deterministic equipment ordering;
- exact AssetId matching state;
- aggregate appearance and equipment fidelity;
- immutable Stage 8 action-lane decisions;
- actionable blockers;
- no Qt, O3DE previewer, file IO, runtime, deployment, save, or process behavior.

A future implementation should expose a small immutable view or service result rather than reproducing these rules in the widget.

### Framework

Framework owns the explicit appearance-binding transaction:

- active workspace and exact game-profile checks;
- active catalog and actor-profile lookup;
- exact pane-entry provenance validation supplied by the Editor client;
- candidate `PopulationActorProfile` update;
- candidate appearance-provenance relationship update;
- complete candidate catalog validation;
- save-before-publish persistence;
- Foundation notification only after durable success;
- rollback to the current published catalog on any failure.

Framework does not invoke O3DE previewers and does not grant runtime authority.

### Editor

Editor owns presentation and user intent:

- actor selection;
- explicit pane-model selection and reload;
- search and product selection;
- live O3DE product display;
- portrait/model candidate classification;
- equipment slot table and selected-equipment preview;
- explicit `Use Selected as Portrait Reference` and `Use Selected as Model Reference` actions;
- fidelity, provenance, staleness, validation, and blocker display;
- no domain-rule duplication, direct file write, catalog promotion, runtime call, deployment, or save mutation.

### O3DE host

O3DE owns:

- Asset Browser product entries;
- registered previewer factories;
- `PreviewerFrame` display behavior;
- product availability in the current Editor session.

Product existence is rendering capability only. It is not authoring permission or FoA runtime compatibility.

### FoA runtime adapters

No runtime-adapter change is part of Stage 8. Any future game-side equip, actor construction, socket attachment, animation, persistence, cleanup, or rollback remains a separately reviewed adapter responsibility.

## Canonical identity rules

### Actor target

The selected target must resolve to one existing catalog record with:

```text
Domain: population
RecordKind: actor
```

It must also have one existing `PopulationActorProfile` before an appearance binding can be changed.

The Stage 8 pane never creates an actor identity or actor profile implicitly.

### Equipment item target

An equipment relationship target is previewable only when it resolves to one existing catalog record with:

```text
Domain: economy
RecordKind: item
```

The item must have one existing `EconomyItemProfile`. Its `AssetRef` is the preferred product reference for equipment preview.

An unresolved `TargetSubjectRef` may remain visible as a blocked relationship, but it cannot be matched to a product by display name, alias, filename, or approximate text.

### Preview product identity

A preview product is identified by the exact O3DE `AssetId` supplied by the matching Asset Browser pane entry.

The pane may display names and cache paths for explanation and search. It must never use them as a binding or join key.

## Appearance binding contract

### Actor profile values

For the first Stage 8 slice:

- `PortraitAssetRef` stores the selected current O3DE product `AssetId` string for an accepted image/texture product;
- `ModelAssetRef` stores the selected current O3DE product `AssetId` string for an accepted model/mesh/prefab product.

The stored value must parse as an O3DE `AssetId`. Empty values remain allowed only under the existing actor-profile contract and are shown as unbound.

### Appearance provenance relationships

Every successful Stage 8 appearance binding also writes one matching first-class catalog relationship.

Accepted relationship kinds are:

```text
actor_uses_portrait_preview
actor_uses_model_preview
```

Relationship shape:

```json
{
  "RelationshipId": "population.appearance.<stable-role-hash>",
  "FromRecordId": "example.pack.actor.guard",
  "ToRecordId": "",
  "TargetSubjectRef": "visual.asset.example.profile.0123456789abcdef",
  "RelationshipKind": "actor_uses_model_preview",
  "EvidenceIds": ["o3de.product-evidence.example"],
  "ResearchStage": "S2",
  "Confidence": "inferred",
  "OperationalRisk": "unknown",
  "ValidationState": "unvalidated",
  "StalenessState": "current",
  "AllowedUsages": [],
  "ForbiddenUsages": ["no_unvalidated_runtime_use"],
  "Attributes": []
}
```

The relationship ID is deterministic from the exact actor record ID and appearance role. It is never derived from a display name.

`TargetSubjectRef` is the pane entry's exact primary source asset-record identity. `EvidenceIds` come from the selected pane entry's product/import evidence. The actor profile's pre-existing actor evidence remains preserved; product evidence is not silently inserted into the actor profile when its subject does not match the actor.

The actor-profile reference and provenance relationship must be updated in one candidate transaction. A failure in profile validation, relationship validation, evidence validation, persistence, or publication leaves both published values unchanged.

This command is explicit authoring. It must never call `PromoteEvidenceToCatalog`, `PromoteCandidateEvidence`, or any automatic evidence-promotion path.

## Equipment relationship contract

Equipment relationships use the existing catalog relationship shape with the actor as source and a canonical item as target.

Accepted first-slice relationship kinds are:

```text
equips_head
equips_torso
equips_hands
equips_legs
equips_feet
equips_main_hand
equips_off_hand
equips_two_hand
equips_back
equips_accessory
equips_other
```

Rules:

- `FromRecordId` resolves to the selected typed actor;
- `ToRecordId` resolves to one typed economy item;
- `TargetSubjectRef`, when also present, must equal the target item's exact subject reference;
- relationship evidence must exist in the active evidence registry and remain bound to the exact active profile, game version, branch, and source fingerprint;
- a relationship may not use a display name as an actor, item, slot, or evidence key;
- one current non-superseded relationship is allowed per actor and slot;
- `equips_two_hand` conflicts with current `equips_main_hand` and `equips_off_hand` relationships;
- duplicate actor/slot relationships are blocked, even when they target the same item;
- unresolved item targets are displayed as blocked and have no product preview;
- superseded, stale, failed, or blocked relationships are visible but cannot contribute a current equipment preview;
- arbitrary relationship attributes do not define attachment transforms in this slice.

The equipment table order is fixed:

```text
head
torso
hands
legs
feet
main_hand
off_hand
two_hand
back
accessory
other
```

This order is presentation and validation order, not a FoA runtime slot claim.

## Asset Browser pane-model input contract

The Stage 8 pane consumes only:

```text
foa-asset-browser-pane-model.json
```

It does not consume raw discovery indexes, thumbnail manifests, neutral handoff documents, conversion documents, raw preview source files, or Asset Processor import-proof documents directly.

A loaded pane model must:

- remain below the active profile's resolved `ExtractedDataPath` after canonical path and symlink resolution;
- use the supported pane-model schema and document kind;
- match the exact active `ProfileId`, `GameVersion`, `Branch`, and `RuntimeTarget`;
- declare that raw conversion and raw preview-source inputs were not consumed;
- retain all operational-authority fields as `false`;
- retain `RequiresExplicitBindingStep=true` and no automatic binding authority;
- contain no more than 10,000 entries;
- remain below 16 MiB;
- contain valid O3DE AssetIds and safe tokenized product cache paths;
- retain the same SHA-256 from load through preview and binding.

A model is cleared when the active profile, game version, branch, runtime target, `ExtractedDataPath`, model path, or model SHA-256 changes.

Before every preview or appearance-binding command, the pane revalidates the active-profile binding and current model-file hash. Stale state blocks the operation rather than reusing cached truth.

## Product classification

### Portrait candidates

A portrait candidate must be a current product whose product kind or cache suffix identifies a bounded image/texture product, including registered O3DE streaming-image products.

A model, material-only, audio, script, or unknown product cannot be applied as a portrait reference.

### Model candidates

A model candidate must be a current product whose product kind identifies a model, mesh, prefab, or other explicitly reviewed static actor-appearance product supported by an O3DE registered previewer.

The first implementation must not accept a product merely because its filename looks like a model. Product kind and registered product identity control acceptance.

Skinned actor reconstruction, skeleton binding, animation graphs, runtime materials, physics, cloth, morph targets, and attachment sockets remain outside the first bounded cohort.

### Equipment candidates

Equipment preview resolves the equipped item's exact `EconomyItemProfile.AssetRef` against pane entries by O3DE AssetId.

When no exact AssetId match exists, the equipment row is `blocked` or `placeholder`; it is never matched by item display name or filename.

## Editor user experience

The Actor and Troop Editor receives one directly integrated top-level tab:

```text
Appearance Preview
```

The implementation should use a dedicated `ActorAppearancePreviewWidget` owned directly by `ActorTroopEditorWidget`. It should not discover the pane through a global Qt event filter.

### Actor and model selection

The tab contains:

- canonical actor filter and selector;
- exact actor identity and profile summary;
- read-only current portrait and model references;
- explicit pane-model chooser and reload control;
- product search field;
- evidence-backed product table;
- current model/profile/hash status;
- action-lane and blocker summary.

### Preview modes

One preview area provides three explicit modes:

```text
Actor model
Portrait
Selected equipment item
```

Each mode displays one exact O3DE product through the registered `PreviewerFrame` path. Switching modes does not bind, persist, or mutate any product.

The equipment mode displays the currently selected equipment row's item product independently. It does not attach the product to the actor model.

### Equipment table

The table contains:

- slot;
- exact relationship ID;
- exact item record ID and subject;
- item profile asset reference;
- relationship validation and staleness;
- relationship evidence IDs;
- product preview state;
- fidelity;
- blockers.

Unresolved and stale rows remain visible for diagnosis.

### Explicit appearance actions

The only authoring controls in the first slice are:

```text
Use Selected as Portrait Reference
Use Selected as Model Reference
```

The buttons remain disabled unless:

- one valid typed actor is selected;
- the actor profile exists;
- one current exact-profile pane model is loaded;
- one accepted current product is selected;
- the product exists in the live Asset Browser;
- the pane-model file still matches its loaded SHA-256;
- the selected entry has a primary source asset-record identity;
- the selected entry has product evidence;
- all applicable path, authority, profile, and product-kind checks pass.

Product selection alone never modifies the actor profile.

## Fidelity model

The UI must display one of the gate fidelity states and its scope.

### `exact`

The exact pane-model AssetId resolves to the live O3DE product, the registered previewer renders it, its provenance is current, and the claim is limited to that O3DE preview product.

`exact` never means exact FoA runtime actor appearance.

### `approximate`

The product is rendered, but runtime material, shader, animation, skeleton, lighting, scale, or other game-side interpretation is not proven.

### `partial`

The actor model and equipment items are independently previewable, but equipment is not attached or composed. This is the expected maximum aggregate equipment fidelity for the first slice.

### `placeholder`

Identity and metadata are present, but no current renderable product is available.

### `unsupported`

The product kind, previewer, or required representation is outside the bounded cohort.

### `blocked`

Profile mismatch, stale model, invalid AssetId, missing evidence, missing actor/item profile, unresolved relationship, duplicate/conflicting slot, failed validation, unsafe path, missing product, or authority escalation prevents preview or binding.

The actor-level summary must separately show:

```text
O3DE preview-product fidelity: <state>
Equipment composition fidelity: partial / placeholder / blocked
FoA runtime appearance: unknown and not tested
```

## Stage 8 action lanes

The Stage 8 service derives an immutable read-only lane table:

```text
appearance_display
portrait_preview
model_preview
equipment_reference_preview
appearance_binding
equipment_relationship_authoring
runtime_equip
save_mutation
```

Rules:

- `appearance_display` may remain available for canonical metadata even when products are blocked;
- preview lanes require current exact-profile products and evidence;
- `appearance_binding` additionally requires explicit user intent and a valid actor profile;
- `equipment_relationship_authoring` is unavailable in this first slice;
- `runtime_equip` is structurally unavailable;
- `save_mutation` is structurally unavailable.

The pane cannot change governance decisions or clear blockers.

## Evidence rules

Appearance and equipment preview remains evidence-bound.

### Actor evidence

The selected actor profile must retain its existing exact actor/template evidence. Changing an appearance reference must not delete, reorder, replace, or fabricate actor evidence.

### Preview-product evidence

The selected pane entry must provide exact product/import evidence and a primary source asset-record identity. That evidence is preserved in the matching appearance-provenance relationship.

### Equipment evidence

An equipment relationship must contain evidence IDs. The complete validation path checks:

- evidence existence;
- source fingerprint;
- active profile, game version, and branch;
- actor or item subject relevance under the reviewed relationship-evidence rule;
- no stale or superseded source;
- no unrelated display-name substitution.

Missing proof fails closed.

## Validation and blockers

Required blockers include:

- no canonical actor selected;
- selected record is not `population/actor`;
- actor profile missing;
- pane model outside `ExtractedDataPath`;
- pane model profile/version/branch/runtime mismatch;
- pane model authority escalation;
- pane-model SHA-256 drift;
- invalid or unsupported O3DE AssetId;
- pane entry missing primary source identity;
- pane entry missing product evidence;
- selected product absent from the live Asset Browser;
- portrait/model product-kind mismatch;
- actor reference does not exactly match a current pane entry;
- equipment relationship source is not the selected actor;
- equipment relationship target is unresolved or not an economy item;
- item profile missing;
- item `AssetRef` missing or invalid;
- equipment item AssetId has no exact pane entry;
- duplicate equipment slot;
- two-hand/main-hand/off-hand conflict;
- relationship validation failed;
- relationship, item, actor, source, or product is stale or superseded;
- private or unsafe path;
- unsupported composite attachment request;
- runtime, save, deployment, promotion, signing, publication, or redistribution request.

Every blocker identifies the actor, slot or appearance role, exact failed requirement, affected lane, and corrective action when known.

## Staleness and invalidation

Preview state becomes stale when any controlling input changes:

- active workspace or profile;
- game version, branch, or runtime target;
- `ExtractedDataPath`;
- pane-model path, SHA-256, tool version, or schema;
- O3DE product AssetId or current product availability;
- actor profile portrait/model reference;
- actor profile evidence;
- equipment relationship target, evidence, validation, staleness, or supersession;
- item profile `AssetRef`;
- referenced actor or item record supersession;
- applicable blocker or governance state.

Foundation notifications invalidate derived equipment and binding state. The widget may preserve harmless filter text and selected actor ID, but it must not preserve a stale product pointer, stale pane-entry pointer, or enabled binding button.

## Security, privacy, and legal boundary

- All pane-model and generated-preview inputs are untrusted.
- Canonical path containment is checked after resolution, including traversal and symlink escape.
- No recursive disk or game-installation scan is introduced.
- No imported content is executed.
- No shell command is constructed from pane-model data.
- No proprietary model, texture, bundle, assembly, screenshot, or extracted payload is committed.
- Fixtures use synthetic project-owned identities, hashes, PNGs, models, and catalog records only.
- Logs and screenshots omit private paths and proprietary content.
- Product cache paths remain tokenized in durable evidence and user-visible diagnostics when possible.

## Accessibility

The first implementation must provide:

- associated labels and buddies for actor, model, search, and mode controls;
- keyboard-accessible product and equipment tables;
- deterministic tab order;
- selectable, word-wrapped identity, provenance, blocker, and status text;
- meaningful accessible names for both the preview frame and binding buttons;
- no severity state conveyed by color alone;
- explicit text for fidelity and unavailable actions;
- no hover-only evidence or blocker details.

## Performance and resource budget

The pane-model limits remain 16 MiB and 10,000 entries.

Implementation rules:

- parse and fingerprint the pane model only on explicit load/reload;
- build AssetId and source-identity indexes once per loaded model;
- resolve actor portrait/model and equipment item references through indexes rather than repeated full scans;
- do not hash files, parse JSON, query all widgets, or scan the catalog during each table repaint;
- refresh derived actor/equipment state once per Foundation notification batch;
- avoid blocking IO in ordinary selection-change handlers;
- retain no live product pointer after model/profile invalidation.

The future implementation PR must include a deterministic performance guard for 10,000 pane entries and a representative bounded equipment set. A specific wall-clock threshold must be established by the implementation test/performance plan rather than invented in this design.

## Persistence and transaction behavior

The first slice changes no durable schema.

An appearance binding transaction performs:

1. resolve the exact actor record and current actor profile;
2. validate the selected pane entry and current model SHA-256;
3. copy the current actor profile;
4. change only the selected portrait or model reference;
5. construct or replace the deterministic matching appearance-provenance relationship;
6. validate actor profile, relationship, evidence, and complete catalog integrity in a candidate catalog;
7. persist the complete candidate catalog;
8. publish the candidate and emit one Foundation notification only after persistence succeeds.

Any failure before step 8 leaves the published catalog unchanged.

The Stage 8 pane performs no direct file write.

Equipment relationships are read-only in this slice, so there is no equipment persistence, removal, or rollback operation.

## Rollback and compatibility

### Design-document rollback

This design document can be reverted without schema or data migration.

### Future implementation rollback

Because the accepted first slice uses existing schema-2 fields and relationships:

- an implementation can be reverted without making schema-2 catalogs unreadable;
- actor portrait/model references remain ordinary actor-profile strings;
- appearance-provenance relationships remain valid generic catalog relationships;
- equipment relationships remain valid generic catalog relationships;
- no runtime or save state exists to roll back.

An implementation PR must be reverted as a complete vertical slice if its widget, service, tests, or validation contracts disagree. Do not retain a binding UI without its atomic provenance transaction or retain relationship semantics without their validators and documentation.

## Required implementation artefacts

An implementation PR following approval must include, at minimum:

- one engine-neutral Stage 8 view/validation service;
- one atomic Framework appearance-binding command;
- one directly integrated `ActorAppearancePreviewWidget`;
- CMake and module ownership updates;
- deterministic synthetic actor/item/product/equipment fixture coverage;
- focused Core and Framework tests;
- pane-model malformed-input and stale-state tests;
- O3DE Editor smoke automation;
- public user and data-format updates where behavior becomes implemented;
- exact-head validation evidence appropriate to the touched source and UI surfaces.

It must not modify tests, validators, workflows, governance, or release gates merely to make the feature pass. New focused tests and validators require explicit scope in the implementation task.

## Test plan for the future implementation

### Core tests

Prove:

- accepted appearance and equipment relationship-kind parsing;
- deterministic slot order;
- actor and item kind validation;
- actor/item profile presence;
- duplicate slot rejection;
- two-hand/main/off-hand conflict rejection;
- unresolved equipment target blocking;
- superseded/stale relationship handling;
- exact AssetId matching only;
- deterministic fidelity and lane derivation;
- runtime and save lanes always unavailable.

### Framework tests

Prove:

- portrait binding updates only `PortraitAssetRef`;
- model binding updates only `ModelAssetRef`;
- actor evidence remains unchanged;
- matching provenance relationship is created or replaced deterministically;
- selected product evidence is preserved on that relationship;
- candidate validation failure publishes nothing;
- persistence failure publishes nothing and emits no notification;
- successful persistence publishes profile and relationship atomically;
- no promotion command is invoked.

### Pane-model and security tests

Prove rejection of:

- absolute/private paths;
- traversal and symlink escape;
- oversized model files;
- more than 10,000 entries;
- malformed JSON;
- unsupported schema/document kind;
- profile/version/branch/runtime mismatch;
- authority escalation;
- invalid AssetIds;
- unsafe cache tokens;
- duplicate pane-entry IDs;
- model SHA-256 drift;
- missing primary source identity or product evidence.

### Editor tests

Prove:

- direct Appearance Preview tab registration exactly once;
- actor selector contains only canonical population actors;
- current portrait/model references and exact identity are visible;
- product search and selection work without binding;
- accepted products display through the registered previewer;
- portrait/model kind restrictions are enforced;
- equipment rows are ordered and blockers are visible;
- selected equipment product previews independently;
- explicit binding buttons are disabled for every blocked condition;
- active profile or model drift clears preview state;
- no equipment-authoring, runtime, save, deployment, promotion, signing, or publication control exists.

### Host and manual evidence

The exact implementation head requires:

- repository static validation;
- reviewed-range whitespace validation;
- pinned-O3DE prerequisites and configure;
- required Editor and Catalog test builds;
- compiled `TaintedGrailModdingSDK.Catalog.Tests` execution with non-zero test count;
- controlled Editor launch;
- synthetic workspace load;
- Appearance Preview interaction evidence;
- save/close/reopen proof for appearance references and provenance relationship;
- explicit confirmation that equipment preview remains independent/reference-only;
- privacy review of logs and screenshots.

No local or host result constitutes FoA runtime sign-off.

## Acceptance criteria

The first Stage 8 implementation is review-ready only when one exact head proves all of the following:

1. One canonical actor can display exact identity, profile, portrait reference, model reference, evidence, validation, and blockers.
2. One exact-profile pane model can be loaded and invalidated safely.
3. One accepted portrait product can be previewed and explicitly bound with atomic provenance.
4. One accepted model product can be previewed and explicitly bound with atomic provenance.
5. One synthetic actor has at least two typed equipment relationships displayed in deterministic slots.
6. Each resolved equipped item can preview its exact product independently.
7. Duplicate, conflicting, unresolved, stale, unsupported, and missing-product equipment states are visibly blocked.
8. Aggregate equipment composition fidelity remains `partial` unless a later approved attachment contract exists.
9. No equipment relationship authoring control is present.
10. No runtime, save, deployment, catalog-promotion, signing, publication, or proprietary-content authority exists.
11. Focused tests, compiled tests, Editor evidence, and documentation belong to the exact reviewed head.

## Completion wording

Allowed before implementation approval and evidence:

- proposed Stage 8 design;
- bounded reference-preview design;
- editor-preview-only;
- schema-compatible proposal;
- not implemented;
- not runtime verified.

Allowed after implementation but before exact-head host/UI evidence:

- implemented source slice;
- preview-gated;
- local checks passed, when named exactly;
- host/UI evidence pending.

Forbidden without separate exact evidence:

- reconstructed FoA actor;
- exact in-game appearance;
- functional game equipment system;
- runtime-compatible loadout;
- full character creator;
- production-ready actor preview;
- save-compatible equipment;
- deployable actor appearance.

## Approval and next process

Maintainer design approval is required before product-source implementation begins.

Approval should explicitly decide:

1. whether existing schema-2 actor fields plus generic relationships are accepted for this slice;
2. whether the closed equipment relationship kinds are accepted;
3. whether independent equipment-product preview with aggregate `partial` fidelity satisfies the bounded first Stage 8 cohort;
4. whether the atomic appearance-provenance relationship is required for every portrait/model binding;
5. whether equipment relationship authoring remains deferred.

After approval, the next researched process is a focused Stage 8 implementation task with explicit source, test, validator, documentation, performance, build, and Editor-evidence scope.

Runtime sign-off is not performed by this design or by its future editor-only implementation.