# Data Formats

## General rules

TG SDK durable documents are UTF-8 JSON produced through O3DE serialization.

Rules:

- top-level durable documents use an explicit schema version;
- field names are case-sensitive;
- stable IDs and exact native references are not display labels;
- unknown schema versions are rejected unless a migration exists;
- secrets and credentials are prohibited;
- private absolute paths should not be distributed publicly;
- runtime permission is never implied by the existence of a document;
- examples must not include copyrighted game content.

The examples below are illustrative. O3DE serialization may omit fields that equal default values.

## Workspace document

Suffix:

```text
*.tgworkspace.json
```

Purpose: editor-owned workspace and exact game-profile configuration.

```json
{
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

Required configured profile fields:

- `ProfileId`
- `InstallPath`
- `GameVersion`
- `Branch`
- `RuntimeTarget`
- `ManagedAssembliesPath`

`RuntimeTarget` must be `Mono` or `IL2CPP`. Mono profiles also require BepInEx version and plugin path for build/deployment planning.

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

Identity rules:

- `PackId` is lowercase and namespaced.
- `OwnerId` is explicit.
- `Version` uses semantic versioning.
- `RuntimeActionsEnabled` must be `false` in editor-owned documents.

Save-impact values:

- `none`
- `compatible`
- `migration`
- `destructive`
- `unknown`

## Source document

Path:

```text
Sources/<source-id>/source.tgsource.json
```

Purpose: immutable description of an imported artifact and its provenance.

```json
{
  "SchemaVersion": 1,
  "Source": {
    "SourceId": "source.profile.fingerprint",
    "Title": "Template diagnostic export",
    "SourceKind": "template-diagnostics",
    "Locator": "/path/to/input.json",
    "Fingerprint": "sha256:...",
    "ProfileId": "foa.mono.current",
    "GameVersion": "exact-version",
    "Branch": "mono",
    "RuntimeTarget": "Mono",
    "ToolName": "Diagnostic Tool",
    "ToolVersion": "1.0.0",
    "ImporterId": "tg.structured-json",
    "ImporterVersion": "1.0.0",
    "CapturedAt": "2026-07-17T10:00:00.000Z",
    "ImportedAt": "2026-07-17T11:00:00.000Z",
    "Limitations": "Read-only capture",
    "MediaType": "application/json",
    "ByteSize": 1234,
    "ImportStatus": "imported"
  },
  "Issues": []
}
```

`Fingerprint` uses lowercase hexadecimal SHA-256:

```text
sha256:<64-hex-digits>
```

A profile/fingerprint pair cannot be registered twice. Import status describes processing, not truth or permission:

- `imported`
- `warning`
- `error`

## Evidence document

Path:

```text
Sources/<source-id>/evidence.tgevidence.json
```

```json
{
  "SchemaVersion": 1,
  "SourceId": "source.profile.fingerprint",
  "SourceFingerprint": "sha256:...",
  "ProfileId": "foa.mono.current",
  "GameVersion": "exact-version",
  "Branch": "mono",
  "Evidence": [
    {
      "EvidenceId": "evidence.fingerprint.1",
      "SourceId": "source.profile.fingerprint",
      "SourceFingerprint": "sha256:...",
      "ProfileId": "foa.mono.current",
      "GameVersion": "exact-version",
      "Branch": "mono",
      "SubjectRef": "subject:example",
      "Claim": "Evidence-backed statement",
      "EvidenceKind": "runtime-observation",
      "Confidence": "unrated",
      "Locator": "/path/to/input.json",
      "RecordPath": "$.evidence[0]",
      "ExtractedAt": "2026-07-17T11:00:00.000Z"
    }
  ],
  "Issues": []
}
```

The document and each child evidence record must exactly match the source ID, fingerprint, profile, game version, and branch.

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

Current severities:

- `warning`
- `error`

Issue codes are stable machine-readable identifiers. Message text may improve without a schema change.

## Structured JSON intake

Accepted shapes:

```json
[
  {
    "subject_ref": "subject:example",
    "claim": "Statement"
  }
]
```

```json
{
  "evidence": [
    {
      "subject_ref": "subject:example",
      "claim": "Statement"
    }
  ]
}
```

```json
{
  "subject_ref": "subject:example",
  "claim": "Statement"
}
```

Required:

- `subject_ref`
- `claim`

Optional:

- `evidence_id`
- `kind`
- `confidence`
- `locator`

## Structured CSV intake

Required header columns:

```text
subject_ref,claim
```

Accepted aliases:

- `subject` for `subject_ref`
- `id` for `evidence_id`
- `evidence_kind` for `kind`

Optional columns:

- `evidence_id`
- `kind`
- `confidence`
- `locator`

CSV supports quoted values and doubled quotes. It is intended for evidence registers, not arbitrary spreadsheet semantics.

## Generic artifact intake

Generic intake creates a source document and an evidence document containing a manual-extraction warning. It does not infer evidence from logs, screenshots, dumps, notes, or unknown formats.

## Canonical catalog document

Path:

```text
Catalog/catalog.tgcatalog.json
```

The catalog is one versioned document bound to the active workspace and exact game profile.

```json
{
  "SchemaVersion": 1,
  "WorkspaceId": "owner.workspace",
  "ProfileId": "foa.mono.current",
  "GameVersion": "exact-version",
  "Branch": "mono",
  "Records": [],
  "Relationships": [],
  "ValidationHistory": []
}
```

Reload rejects mismatched workspace ID, profile ID, game version, or branch.

### Catalog record

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
  "ResearchStage": "reviewed",
  "Confidence": "documented",
  "OperationalRisk": "unknown",
  "ValidationState": "unvalidated",
  "AllowedUsages": [],
  "ForbiddenUsages": ["no_unvalidated_runtime_use"],
  "EvidenceIds": ["evidence.fingerprint.1"],
  "MissingRefs": [],
  "ConflictRefs": [],
  "Tags": ["example"],
  "CreatedAt": "2026-07-17T12:00:00.000Z",
  "UpdatedAt": "2026-07-17T12:00:00.000Z",
  "SupersededByRecordId": ""
}
```

Required canonical fields:

- `RecordId`
- `Domain`
- `RecordKind`
- `SubjectRef`
- `IdentityKind`
- at least one `EvidenceId`

Identity-specific requirements:

- `native` requires `NativeRefExact` and no custom owner claim;
- `synthetic` requires an existing `OwnerPackId` and no borrowed native ref;
- `composite` and `source_scoped` require explicit reviewed meaning and evidence.

The database rejects duplicate record IDs and duplicate non-empty exact native references. Display names may repeat and are never deduplication keys.

Promotion creates `unvalidated` records and cannot populate `AllowedUsages`.

### Catalog relationship

```json
{
  "RelationshipId": "relationship.example.contains",
  "FromRecordId": "record.parent",
  "ToRecordId": "record.child",
  "TargetSubjectRef": "",
  "RelationshipKind": "contains",
  "EvidenceIds": ["evidence.fingerprint.2"],
  "ValidationState": "unvalidated",
  "Attributes": []
}
```

A relationship requires:

- stable relationship ID;
- existing source record ID;
- relationship kind;
- target record ID or unresolved target subject reference;
- at least one evidence ID.

When `ToRecordId` is present, it must identify an existing catalog record. `TargetSubjectRef` supports a reviewed unresolved target without inventing a canonical ID.

### Catalog validation event

```json
{
  "ValidationId": "validation.record.example.1",
  "RecordId": "native.item.example",
  "State": "validated",
  "Method": "runtime-observation",
  "Validator": "validator-id",
  "CheckedAt": "2026-07-17T13:00:00.000Z",
  "ProfileId": "foa.mono.current",
  "GameVersion": "exact-version",
  "EvidenceIds": ["evidence.fingerprint.3"],
  "Notes": "Observed in the configured build."
}
```

Validation history requires:

- stable validation ID;
- existing record ID;
- state;
- method;
- check time;
- exact profile/version binding;
- evidence links when the method depends on evidence.

History is retained separately from the record's current validation state.

## Compatibility and migration

Backward-compatible examples:

- adding an optional field with a safe default;
- adding a new issue or blocker code;
- adding a query filter;
- adding an importer that does not reinterpret existing documents.

Breaking examples:

- changing identity semantics;
- renaming/removing a field;
- changing a field type;
- changing fingerprint or profile-binding scope;
- changing exact-ref uniqueness;
- changing pack ownership requirements;
- merging previously distinct record kinds.

Breaking changes require:

- schema version increment;
- migration tool or explicit unsupported-version error;
- old/new fixtures and tests;
- changelog, user-guide, and catalog-guide updates;
- release notes and rollback guidance.
