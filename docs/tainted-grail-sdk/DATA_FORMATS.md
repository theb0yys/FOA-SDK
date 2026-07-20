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
  "SchemaVersion": 1,
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
  "RecipeOutputs": []
}
```

The four economy arrays are optional schema-1 extensions. Older schema-1 documents without them load as empty economy collections.

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
