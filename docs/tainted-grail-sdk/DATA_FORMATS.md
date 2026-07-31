# Data Formats

## General rules

TG SDK durable documents are UTF-8 JSON produced through reviewed persistence services.

All durable formats follow these rules:

- top-level documents use an explicit schema version when the format defines one;
- field names are case-sensitive;
- stable IDs and exact native references are not display labels;
- unknown schema versions are rejected unless a migration exists;
- secrets and credentials are prohibited;
- private absolute paths should not be distributed publicly;
- runtime permission is never implied by the existence of a document;
- imported evidence, reviewed records, validation, and permission remain separate;
- examples must not contain copyrighted game assets or proprietary source code.

O3DE reflection versions are implementation metadata and are not durable document schema versions.

## QuestDefinition document

Suffix:

```text
*.tgquest.json
```

Purpose: portable, engine-neutral quest definition data. Version 1 describes authored quest shape
only and does not grant runtime execution, editor mutation, save mutation, deployment, asset
extraction, or Fall of Avalon runtime proof.

Schema identity:

```text
Schema ID: foa.quest-definition
Schema version: 1
Canonical fingerprint: lowercase sha256 over the canonical JSON projection
```

Top-level fields:

- `schema`;
- `schema_version`;
- `quest_id`;
- `content_version`;
- `owner_pack_id`;
- `owner_module_id`;
- `display`;
- `lifecycle`;
- `roles`;
- `phases`;
- `objectives`;
- `transitions`;
- `conditions`;
- `actions`;
- `outcomes`;
- `binding_requirements`;
- `minimum_sdk_version`;
- `compatibility_tags`;
- `authority`;
- optional `quest_fingerprint`.

Schema-1 rules:

- unknown public fields and unknown schema versions are rejected;
- IDs are stable contract IDs, not display names, native object references, paths, URIs, GUIDs, or
  engine handles;
- roles, phases, objectives, transitions, conditions, actions, outcomes, and binding requirements
  use duplicate-free IDs;
- transitions, objectives, phases, outcomes, and binding requirements must reference declared IDs;
- exactly one entry phase and at least one terminal outcome are required;
- ambiguous transitions and cycles without explicit repeat policy are rejected;
- condition and action type IDs must come from the V1 registered vocabulary;
- strings, arrays, document size, and nesting surfaces are bounded;
- canonical JSON sorts set-like arrays and excludes `quest_fingerprint` from its own projection;
- `authority.runtime_execution_allowed`, `authority.editor_mutation_allowed`,
  `authority.save_mutation_allowed`, `authority.deployment_allowed`, and
  `authority.asset_extraction_allowed` are always `false`.

## QuestBindingManifest document

Suffix:

```text
*.tgquestbindings.json
```

Purpose: portable, engine-neutral binding data that records which reviewed catalog,
evidence, and permission references satisfy a `QuestDefinition` V1 binding requirement.
Version 1 is inert contract data only.

Schema identity:

```text
Schema ID: foa.quest-binding-manifest
Schema version: 1
Canonical fingerprint: lowercase sha256 over the canonical JSON projection
```

Top-level fields:

- `schema`;
- `schema_version`;
- `manifest_id`;
- `quest_id`;
- `content_version`;
- `owner_pack_id`;
- `owner_module_id`;
- `quest_definition_fingerprint`;
- `catalog_id`;
- `catalog_fingerprint`;
- `profile_id`;
- `game_version`;
- `branch`;
- `runtime_target`;
- `role_bindings`;
- `location_bindings`;
- `anchor_bindings`;
- `item_bindings`;
- `reward_bindings`;
- `dialogue_bindings`;
- `journal_bindings`;
- `minimum_sdk_version`;
- `compatibility_tags`;
- `authority`;
- optional `manifest_fingerprint`.

Schema-1 rules:

- unknown public fields and unknown schema versions are rejected;
- `manifest_id`, `quest_id`, pack/module IDs, binding IDs, requirement IDs,
  role IDs, subject kinds, usage IDs, catalog IDs, evidence IDs, source IDs,
  permission IDs, validation IDs, and compatibility tags use stable contract IDs;
- `binding_kind` is a bounded discriminator matching its containing binding array;
- binding rows are duplicate-free and canonical JSON sorts binding arrays, evidence
  refs, permission refs, and compatibility tags on copies;
- `quest_definition_fingerprint`, `catalog_fingerprint`, evidence
  `source_fingerprint`, and optional `manifest_fingerprint` are lowercase
  `sha256:<64-hex-digits>` values;
- `manifest_fingerprint` is excluded from its own canonical projection;
- binding rows may validate by value against an immutable supplied `QuestDefinition`
  snapshot and must match declared binding requirements when that snapshot is supplied;
- catalog references bind stable catalog record/context identifiers and must not embed
  native O3DE, Unity, FoA runtime handles, absolute paths, traversal paths, URIs, or
  GUID-like values;
- evidence references require source IDs, source fingerprints, and matching
  profile/build context;
- permission references must match subject kind, subject ID, and usage; missing,
  denied, unresolved, stale, or mismatched permission status fails closed;
- all authority flags stay false: `runtime_execution_allowed`,
  `editor_mutation_allowed`, `save_mutation_allowed`, `deployment_allowed`,
  `catalog_mutation_allowed`, `evidence_promotion_allowed`,
  `permission_grant_allowed`, and `asset_extraction_allowed`.

The document does not query live catalog services, repair catalog state, promote evidence,
grant permission, launch FoA, access saves, mutate editor state, deploy packages, or prove
runtime behavior.

## External-tool interchange Gate 0 envelopes

Gate 0 defines four canonical Core-only envelopes:

- `ExternalToolHandoffV1` binds one exact provider, application, native extension, command,
  toolchain lock, and safe relative staging root; future qualification/source bindings are optional and
  absent-or-complete;
- `UnityConversionRequestV1` binds the exact canonical handoff and repeats its provider,
  application, and extension identities without selecting their eventual Gate 8 values;
- `ExternalToolExecutionResultV1` records only a typed `not_attempted` result;
- `UnityConversionResultV1` binds the exact request and external-tool result and likewise records
  only `not_attempted`.

Every envelope uses `ContractVersion` 1 and a lowercase `sha256:<64-hex-digits>` fingerprint over
its fixed-order canonical JSON projection. An object's own fingerprint is excluded from that
projection. Embedded upstream canonical JSON and its fingerprint are included and must exactly
match the supplied upstream object. Set-like evidence, asset, and reason-code arrays are sorted on
copies for canonical output, and duplicates fail validation.

Provider and extension package versions use strict semantic versions. Native application versions
use a bounded exact token because application-native values such as Unity `2022.3.22f1` are not
semantic versions. The only path in the handoff envelope is a contained package-relative staging
identity; executable paths and Unity project paths are not part of these Core contracts.

Gate 0 does not select the Gate 5 interchange schema: `InterchangeSchemaVersion` is `0`. Empty
qualification, source-package, source-manifest, and requested-asset sets make no claim that their later
owners exist or have accepted an input. These envelopes expose canonical projections and bindings only; Gate
0 adds no JSON parser, persistence service, file suffix, or durable registry.

`ExecutionAllowed`, `BuildAllowed`, and `DeploymentAllowed` are always `false`. Results cannot
claim an attempted conversion, exit status, process timing, output manifest, output package, loss
report, project mutation, build, or deployment. A whole-second UTC capture time and typed reason
code explain the disabled result, but no process is launched, no file is opened or written, and no
candidate evidence is persisted or promoted.

V1 is permanently inert. A later execution gate must introduce a new contract version and may not
relax the V1 validators. Provider and extension identities remain exact handoff bindings in V1;
their final product values are resolved only by the later provider-selection gate.

## Terrain heightmap document

Suffix:

```text
*.tgheightmap.json
```

Purpose: local-only, editor-owned terrain heightmap authoring state for user-exported heightmap
inputs. Version 1 does not identify native FoA map objects, grant runtime permission, deploy files,
publish payloads, or promote evidence.

Schema identity:

```text
Schema ID: foa.terrain-heightmap
Schema version: 1
Canonical payload: unsigned 16-bit integer tiles
Tile byte order: little-endian
Tile storage order: row-major
```

Top-level fields:

- `schema`;
- `schema_version`;
- `document_id`;
- `map_identity`;
- `profile_binding`;
- `source_binding`;
- `grid`;
- `sample_encoding`;
- `vertical_mapping`;
- `coordinate_space`;
- `tiles`;
- `provenance`;
- `legal_state`;
- `revision`;
- `local_payload_state`;
- `authority`.

Schema-1 rules:

- unknown schema versions and unknown required enum values are rejected;
- `map_identity` uses a stable synthetic terrain map ID, and public aliases do not become native
  map identities;
- `profile_binding` binds the document to the active profile ID, game version, branch, runtime
  target, and profile fingerprint;
- `source_binding` records a redacted, local-only user-exported source observation and must not
  contain private absolute input paths;
- `grid` dimensions are positive and bounded, and total-sample multiplication must not overflow;
- `sample_encoding` is canonical `u16`, little-endian, row-major, unsigned, 16-bit payload data;
- `vertical_mapping` uses finite metres and requires `max_height_metres > min_height_metres`;
- `coordinate_space` records handedness, axes, row-zero orientation, sample-position semantics, and
  a deterministic transform;
- `tiles` must exactly cover the grid without gaps, overlaps, duplicates, path escapes, case
  collisions, private paths, unsupported suffixes, or byte-size mismatches;
- canonical height conversion is
  `height_metres = min_height_metres + (uint16_sample / 65535) * (max_height_metres - min_height_metres)`;
- workspace-derived manifests, local tile payloads, source observations, and preview projections
  remain excluded from official package contents by default;
- all authority flags stay false: `runtime_use_allowed`, `deployment_allowed`,
  `publication_allowed`, `packaging_allowed`, `game_write_allowed`, and
  `evidence_promotion_allowed`.

Accepted first-route inputs are explicit user-selected 16-bit greyscale PNG, 16-bit greyscale TIFF,
and raw U16 grids with mandatory sidecars. Raw sidecars must provide dimensions, byte order,
vertical range, spacing, coordinate basis, row-zero orientation, and sample-position semantics; the
importer must not guess these from file size or appearance. Direct Unity bundles, scenes, `.assets`,
`.resS`, Addressables bundles, executables, assemblies, saves, and protected containers are not valid
heightmap inputs.

## Workspace document

Suffix:

```text
*.tgworkspace.json
```

Purpose: editor-owned workspace and exact game-profile configuration.

```json
{
  "SchemaVersion": 1,
  "WorkspaceId": "owner.workspace",
  "DisplayName": "My TG Workspace",
  "RootPath": "/authoring/MyWorkspace",
  "OutputPath": "/authoring/MyWorkspace/Build",
  "StagingPath": "/authoring/MyWorkspace/Staging",
  "DeploymentPath": "/authoring/MyWorkspace/Deployment",
  "ActiveGameProfileId": "foa.mono.current",
  "GameProfiles": [
    {
      "ProfileId": "foa.mono.current",
      "DisplayName": "FoA Mono Current",
      "InstallPath": "/games/FoA",
      "GameVersion": "exact-version",
      "Branch": "mono",
      "RuntimeTarget": "Mono",
      "UnityVersion": "exact-unity-version",
      "BepInExVersion": "exact-bepinex-version",
      "ManagedAssembliesPath": "/games/FoA/Tainted Grail_Data/Managed",
      "PluginPath": "/games/FoA/BepInEx/plugins",
      "DiagnosticsPath": "/authoring/MyWorkspace/Diagnostics",
      "ExtractedDataPath": "/authoring/MyWorkspace/Extracted",
      "DlcScopes": ["base-game"]
    }
  ]
}
```

Schema-1 rules:

- `WorkspaceId` and every `ProfileId` are lowercase namespaced stable IDs;
- profile IDs are unique and `ActiveGameProfileId` binds to exactly one profile;
- root, output, staging and deployment paths are required;
- a relative root is resolved against the canonical workspace-document directory;
- output, staging, deployment, diagnostics and extracted-data paths remain inside the canonical workspace root;
- managed assemblies and the Mono plugin path remain inside the canonical installation path;
- `RuntimeTarget` is `Mono` or `IL2CPP`; Mono profiles also require the BepInEx version and plugin path.

A workspace without `SchemaVersion` is legacy schema 0. It migrates in memory only when every schema-1 identity, binding and path-independent structural rule can be validated without guessing. Unsafe legacy documents and unknown schema versions are rejected. Saving a migrated workspace emits schema 1.

## Pack manifest

Suffix:

```text
*.tgpack.json
```

Required location: inside the active workspace root.

```json
{
  "SchemaVersion": 1,
  "PackId": "owner.pack-name",
  "DisplayName": "Pack Name",
  "OwnerId": "owner",
  "Version": "0.1.0",
  "TargetGameVersion": "exact-version",
  "TargetBranch": "mono",
  "CompatibleGameVersions": [],
  "RequiredCoreVersion": "0.1.0",
  "RequiredAdapterVersion": "0.1.0",
  "SaveImpact": "unknown",
  "DlcScopes": ["base-game"],
  "Dependencies": [],
  "RequiredMods": [],
  "Incompatibilities": [],
  "ContentDefinitionPaths": ["Content/items.json"],
  "AssetPaths": [],
  "LocalisationPaths": [],
  "BuildConfiguration": "Profile",
  "ReleaseChannel": "development",
  "RuntimeActionsEnabled": false
}
```

Rules:

- `PackId` is lowercase and namespaced;
- `OwnerId` is explicit;
- `Version` uses semantic versioning;
- `RuntimeActionsEnabled` must be `false` in editor-owned manifests;
- save impact is `none`, `compatible`, `migration`, `destructive`, or `unknown`.

## Source document

Path:

```text
Sources/<source-id>/source.tgsource.json
```

Purpose: immutable provenance for an imported artifact.

```json
{
  "SchemaVersion": 1,
  "Source": {
    "SourceId": "source.profile.fingerprint",
    "Title": "Template diagnostic export",
    "SourceKind": "template-diagnostics",
    "Locator": "/path/to/input.json",
    "Fingerprint": "sha256:<64-hex-digits>",
    "ProfileId": "foa.mono.current",
    "GameVersion": "exact-version",
    "Branch": "mono",
    "RuntimeTarget": "Mono",
    "ToolName": "Diagnostic Tool",
    "ToolVersion": "1.0.0",
    "ImporterId": "tg.structured-json",
    "ImporterVersion": "1.0.0",
    "CapturedAt": "2026-07-17T10:00:00Z",
    "ImportedAt": "2026-07-17T11:00:00Z",
    "Limitations": "Read-only capture",
    "MediaType": "application/json",
    "ByteSize": 1234,
    "ImportStatus": "imported"
  },
  "Issues": []
}
```

A profile/fingerprint pair cannot be registered twice. Import status describes processing only, not factual validity or runtime permission.

## Evidence document

Path:

```text
Sources/<source-id>/evidence.tgevidence.json
```

```json
{
  "SchemaVersion": 1,
  "SourceId": "source.profile.fingerprint",
  "SourceFingerprint": "sha256:<64-hex-digits>",
  "ProfileId": "foa.mono.current",
  "GameVersion": "exact-version",
  "Branch": "mono",
  "Evidence": [
    {
      "EvidenceId": "evidence.fingerprint.1",
      "SourceId": "source.profile.fingerprint",
      "SourceFingerprint": "sha256:<64-hex-digits>",
      "ProfileId": "foa.mono.current",
      "GameVersion": "exact-version",
      "Branch": "mono",
      "SubjectRef": "subject:example",
      "Claim": "Evidence-backed statement",
      "EvidenceKind": "runtime-observation",
      "Confidence": "unrated",
      "Locator": "/path/to/input.json",
      "RecordPath": "$.evidence[0]",
      "ExtractedAt": "2026-07-17T11:00:00Z"
    }
  ],
  "Issues": []
}
```

The evidence document and every child record must match the source ID, fingerprint, profile, game version, and branch.

## Structured intake shapes

Structured JSON accepts:

- a root array of evidence-shaped objects;
- an object containing an `evidence` array;
- one evidence-shaped root object.

Required JSON fields are `subject_ref` and `claim`. Optional fields are `evidence_id`, `kind`, `confidence`, and `locator`.

Structured CSV requires:

```text
subject_ref,claim
```

Optional columns are `evidence_id`, `kind`, `confidence`, and `locator`. Generic artifact intake creates provenance and a manual-extraction issue but does not infer evidence.

Structured JSON and CSV must decode as valid UTF-8 without replacement
characters. CSV follows quoted-field rules including doubled quotes and embedded
newlines, rejects unterminated or misplaced quotes, and is limited to 100,000
evidence rows in addition to the 64 MiB source-byte limit.

All durable contract timestamps use whole-second UTC precision exactly as
`YYYY-MM-DDTHH:MM:SSZ`. Fractional seconds are rejected so chronological
comparisons remain unambiguous.

## Import issue

```json
{
  "IssueId": "issue.source-id.1",
  "Severity": "error",
  "Code": "schema.evidence-required-fields",
  "Message": "Evidence entries require subject_ref and claim.",
  "Locator": "/path/to/input.json",
  "RecordPath": "$.evidence[0]",
  "Line": 0
}
```

Severity is currently `warning` or `error`. Codes are stable machine-readable identifiers.

## Canonical catalog document

Path:

```text
Catalog/catalog.tgcatalog.json
```

The document is bound to one workspace and exact game profile:

```json
{
  "SchemaVersion": 2,
  "WorkspaceId": "owner.workspace",
  "ProfileId": "foa.mono.current",
  "GameVersion": "exact-version",
  "Branch": "mono",
  "Records": [],
  "Relationships": [],
  "ValidationHistory": [],
  "GovernanceHistory": [],
  "EconomyItems": [],
  "EconomyRecipes": [],
  "RecipeIngredients": [],
  "RecipeOutputs": [],
  "ActorProfiles": [],
  "TroopProfiles": [],
  "TroopMembers": []
}
```

Schema 2 is the current writable catalog format. The four economy arrays remain compatible with schema 1;
older schema-1 documents without them load as empty economy collections. Schema 2 adds the three population
arrays shown above.

Schema-1 migration is read-only and fail-closed:

1. the original schema version is detected before migration;
2. the existing catalog fields load and the three population collections initialise empty;
3. a schema-1 document with non-empty population collections is rejected rather than reinterpreted;
4. legacy validation and governance compatibility rules run without changing the detected schema version;
5. the complete candidate is validated against the active workspace, profile, evidence registry, and catalog
   integrity rules before successful bound replacement;
6. only successful bound replacement followed by `BuildDocument` produces a schema-2 document;
7. the next successful catalog save writes that schema-2 document, including when every population collection
   is empty.

A loaded schema-1 candidate remains schema 1 after compatibility normalization.
Directly saving that load result is refused.
Catalog saving does not publish schema 1. Unknown versions, malformed schema-version values, failed migration,
and failed persistence are rejected without replacing the published catalog. Migration preserves existing
records, relationships, validation history, governance history, economy collections, and their stable order.
Plain catalog documents require an explicit `SchemaVersion`. A legacy O3DE `JsonSerialization` envelope that
predates a nested catalog schema is treated only as a schema-1 migration input. Current saves always emit the
plain schema-2 document with explicit empty collections, not a new O3DE envelope.

Reload rejects a mismatched workspace ID, profile ID, game version, or branch.

## Catalog record

```json
{
  "RecordId": "native.item.example",
  "OwnerPackId": "",
  "Domain": "economy",
  "RecordKind": "item",
  "SubjectRef": "subject:item:example",
  "NativeRefExact": "00000000-0000-0000-0000-000000000000",
  "IdentityKind": "native",
  "DisplayName": "Example Item",
  "Aliases": ["Example"],
  "SourceScopedRefs": [],
  "ResearchStage": "S5",
  "Confidence": "documented",
  "OperationalRisk": "unknown",
  "ValidationState": "unvalidated",
  "StalenessState": "unknown",
  "AllowedUsages": [],
  "ForbiddenUsages": ["no_unvalidated_runtime_use"],
  "EvidenceIds": ["evidence.fingerprint.1"],
  "MissingRefs": [],
  "ConflictRefs": [],
  "Tags": ["example"],
  "CreatedAt": "2026-07-17T12:00:00Z",
  "UpdatedAt": "2026-07-17T12:00:00Z",
  "SupersededByRecordId": ""
}
```

Required fields are record ID, domain, record kind, subject reference, identity kind, and at least one evidence ID.

Identity rules:

- `native` requires an exact native ref and no custom owner claim;
- `synthetic` requires an owning pack and no borrowed native ref;
- `composite` and `source_scoped` require explicit reviewed meaning and evidence.

Duplicate record IDs and duplicate non-empty exact native references are rejected. Display names may repeat and are never identity keys.

## Catalog relationship

```json
{
  "RelationshipId": "relationship.item.vendor",
  "FromRecordId": "native.item.example",
  "ToRecordId": "vendor.example",
  "TargetSubjectRef": "",
  "RelationshipKind": "sold_by",
  "EvidenceIds": ["evidence.fingerprint.2"],
  "ResearchStage": "S2",
  "Confidence": "inferred",
  "OperationalRisk": "unknown",
  "ValidationState": "unvalidated",
  "StalenessState": "unknown",
  "AllowedUsages": [],
  "ForbiddenUsages": ["no_unvalidated_runtime_use"],
  "MissingRefs": [],
  "ConflictRefs": [],
  "Attributes": [],
  "CreatedAt": "2026-07-17T12:00:00Z",
  "UpdatedAt": "2026-07-17T12:00:00Z",
  "SupersededByRelationshipId": ""
}
```

A relationship requires a stable ID, existing source record, kind, exactly one target record or target subject reference, and evidence.

## Validation history

```json
{
  "ValidationId": "validation.record.example.1",
  "SubjectKind": "record",
  "SubjectId": "native.item.example",
  "RecordId": "",
  "State": "validated",
  "Method": "runtime-observation",
  "Validator": "validator-id",
  "CheckedAt": "2026-07-17T13:00:00Z",
  "ProfileId": "foa.mono.current",
  "GameVersion": "exact-version",
  "Branch": "mono",
  "EvidenceIds": ["evidence.fingerprint.3"],
  "Notes": "Observed in the configured build."
}
```

`SubjectKind` is `record` or `relationship`. `RecordId` remains only for legacy schema-1 record validation entries. New entries use `SubjectKind` and `SubjectId`.

Validation history is append-only. A validation result does not grant permission.

## Governance history

```json
{
  "EventId": "governance.record.example.permission.1",
  "SubjectKind": "record",
  "SubjectId": "native.item.example",
  "Axis": "permission",
  "PreviousValue": "unset",
  "NewValue": "allow",
  "Usage": "existing_item_grant",
  "EvidenceIds": ["evidence.fingerprint.3"],
  "ValidationIds": ["validation.record.example.1"],
  "Reviewer": "reviewer-id",
  "DecidedAt": "2026-07-17T13:05:00Z",
  "Notes": "Narrow existing-item lane only."
}
```

Governance events preserve reviewed maturity, confidence, risk, staleness, permission, prohibition, and supersession decisions. An allowed usage requires validated proof for the same subject.

## Economy item profile

Array: `EconomyItems`

```json
{
  "RecordId": "native.item.example",
  "Category": "material",
  "Subtype": "crafting-component",
  "StackLimit": 20,
  "Weight": 0.1,
  "BaseValue": 12.0,
  "Rarity": "common",
  "Quality": "standard",
  "Durability": 0.0,
  "QuestItem": false,
  "UniqueItem": false,
  "HiddenItem": false,
  "LocalisationNameRef": "loc.item.example.name",
  "LocalisationDescriptionRef": "loc.item.example.description",
  "IconRef": "icon:item:example",
  "AssetRef": "asset:item:example",
  "Tags": ["material"],
  "EvidenceIds": ["evidence.item.profile"]
}
```

Rules:

- `RecordId` references an existing canonical `economy/item` record;
- evidence exists and matches the canonical item subject;
- weight, base value, and durability are non-negative;
- quest and unique flags feed economy blockers and do not grant permission.

## Economy recipe profile

Array: `EconomyRecipes`

```json
{
  "RecordId": "native.recipe.example",
  "RecipeType": "handcrafting",
  "RecipeTab": "general",
  "StationRecordIds": ["economy.station.handcrafting"],
  "UnlockMode": "learned",
  "UnlockSubjectRefs": ["subject:book:example"],
  "DuplicateKey": "native.recipe.example",
  "PersistenceMode": "native_template",
  "HiddenRecipe": false,
  "EvidenceIds": ["evidence.recipe.profile"]
}
```

Supported persistence descriptions are `unknown`, `native_template`, `runtime_append`, and `custom_template`.

Rules:

- `RecordId` references an existing canonical `economy/recipe` record;
- station IDs reference canonical economy station, crafting-station, or interaction-target records;
- native recipes cannot use `custom_template`;
- synthetic recipes cannot claim `native_template`;
- recipe type, duplicate key, persistence mode, and evidence are required.

Persistence mode describes research and planning only. It does not permit runtime append or registration.

## Recipe ingredient join

Array: `RecipeIngredients`

```json
{
  "LinkId": "ingredient.recipe.example.1",
  "RecipeRecordId": "native.recipe.example",
  "ItemRecordId": "native.item.example",
  "ItemSubjectRef": "",
  "Quantity": 2,
  "AlternativeGroup": "",
  "Consumed": true,
  "Conditions": [],
  "EvidenceIds": ["evidence.recipe.ingredient.1"]
}
```

Rules:

- recipe ID references an existing typed recipe profile;
- exactly one item record ID or unresolved item subject reference is present;
- resolved item IDs reference canonical `economy/item` records;
- quantity is positive;
- evidence exists and belongs to the recipe, resolved item, or explicit unresolved subject.

## Recipe output join

Array: `RecipeOutputs`

```json
{
  "LinkId": "output.recipe.example.1",
  "RecipeRecordId": "native.recipe.example",
  "ItemRecordId": "native.item.example",
  "ItemSubjectRef": "",
  "Quantity": 1,
  "Chance": 1.0,
  "ByProduct": false,
  "Conditions": [],
  "EvidenceIds": ["evidence.recipe.output.1"]
}
```

Rules:

- recipe ID references an existing typed recipe profile;
- exactly one item record ID or unresolved item subject reference is present;
- quantity is positive;
- chance is greater than `0` and no greater than `1`;
- evidence exists and belongs to the recipe, output item, or explicit unresolved subject.

## Population actor profile

Array: `ActorProfiles`

```json
{
  "RecordId": "population.actor.example",
  "ActorKind": "npc",
  "Archetype": "guard",
  "TemplateRecordId": "population.template.guard",
  "TemplateSubjectRef": "subject:population:template:guard",
  "MinimumLevel": 5,
  "MaximumLevel": 10,
  "UniqueActor": false,
  "EssentialActor": false,
  "PersistentActor": false,
  "LocalisationNameRef": "loc.actor.example.name",
  "LocalisationDescriptionRef": "loc.actor.example.description",
  "PortraitAssetRef": "asset:portrait:actor:example",
  "ModelAssetRef": "asset:model:actor:example",
  "Tags": ["guard"],
  "EvidenceIds": ["evidence.population.actor.example"]
}
```

Rules:

- `RecordId` references an existing canonical `population/actor` record;
- `ActorKind` is `npc`, `creature`, `animal`, `construct`, or `other`;
- `Archetype` is required and bounded;
- a level range is either absent as two zero values or ordered, positive, and no greater than `1000`;
- a resolved template references a canonical `population/template` or `population/actor_template` record;
- resolved and unresolved template bindings may coexist only when their exact subject references agree;
- tags and evidence IDs are non-empty, bounded, unique, and emitted in deterministic order.

Lifecycle flags and asset or localisation references are authoring and planning metadata. They do not grant
spawn, runtime, deployment, or save-mutation authority.

## Population troop profile

Array: `TroopProfiles`

```json
{
  "RecordId": "population.troop.example",
  "TroopKind": "patrol",
  "LeaderActorRecordId": "population.actor.example",
  "LeaderActorSubjectRef": "subject:population:actor:example",
  "MinimumSize": 2,
  "MaximumSize": 4,
  "Formation": "line",
  "Tags": ["patrol"],
  "EvidenceIds": ["evidence.population.troop.example"]
}
```

Rules:

- `RecordId` references an existing canonical `population/troop` record;
- `TroopKind` is `party`, `patrol`, `encounter_group`, `reinforcement`, or `other`;
- size bounds are positive, ordered, and no greater than `1000`;
- an optional resolved leader references a canonical `population/actor` record;
- resolved and unresolved leader bindings may coexist only when their exact subject references agree;
- tags and evidence IDs are bounded, unique, and emitted in deterministic order;
- a troop has at least one typed member, and a declared leader has exactly one matching `leader` member row.

## Population troop member

Array: `TroopMembers`

```json
{
  "LinkId": "population.member.example.leader",
  "TroopRecordId": "population.troop.example",
  "ActorRecordId": "population.actor.example",
  "ActorSubjectRef": "subject:population:actor:example",
  "Role": "leader",
  "MinimumCount": 1,
  "MaximumCount": 1,
  "Weight": 1.0,
  "Required": true,
  "Conditions": ["daytime"],
  "EvidenceIds": ["evidence.population.member.example.leader"]
}
```

Rules:

- `LinkId` is the stable membership identity and `TroopRecordId` binds an existing typed troop;
- at least one exact actor record or unresolved actor subject is present;
- resolved actor IDs reference canonical `population/actor` records, and dual bindings must agree;
- `Role` is `leader`, `melee`, `ranged`, `support`, `specialist`, or `other`;
- count bounds are positive, ordered, and no greater than `1000`;
- weight is finite and non-negative;
- conditions and evidence IDs are bounded, unique, and emitted in deterministic order;
- one troop cannot contain duplicate rows for the same exact actor subject or more than one typed leader row;
- the combined member ranges overlap the troop's declared size range.

Every population profile and member row remains evidence-bound during complete-catalog validation. Population
data is authoring state only; no population contract invokes FoA, spawns an actor, mutates a save, or grants a
runtime permission.

## Compatibility and migration

Backward-compatible examples:

- adding an optional field with a safe default;
- adding an optional typed-domain array;
- adding a query filter, blocker code, or importer;
- adding a new relationship or permission vocabulary without reinterpreting old values.

Breaking examples:

- changing identity semantics;
- renaming or removing a field;
- changing a field type;
- changing fingerprint or profile-binding scope;
- changing exact-ref uniqueness or pack ownership;
- changing quantity, chance, or join identity semantics;
- merging previously distinct record kinds.

Breaking changes require:

- schema version increment;
- migration tool or explicit unsupported-version error;
- old/new fixtures and tests;
- changelog and user-guide updates;
- release notes and rollback guidance.
