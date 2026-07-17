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

A configured profile requires `ProfileId`, installation, exact game version, branch, runtime target, and managed-assemblies path. `RuntimeTarget` must be `Mono` or `IL2CPP`.

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

- `PackId` is lowercase and namespaced;
- `OwnerId` is explicit;
- `Version` uses semantic versioning;
- `RuntimeActionsEnabled` must be `false` in editor-owned documents.

Save-impact values are `none`, `compatible`, `migration`, `destructive`, and `unknown`.

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

`Fingerprint` uses `sha256:<64-hex-digits>`. A profile/fingerprint pair cannot be registered twice. Import status describes processing, not factual validity or runtime permission.

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

The evidence document and child records must match the source ID, fingerprint, profile, version, and branch.

## Structured intake shapes

Structured JSON accepts a root array, an `evidence` array, or one evidence-shaped object. Required fields are `subject_ref` and `claim`; optional fields are `evidence_id`, `kind`, `confidence`, and `locator`.

Structured CSV requires `subject_ref,claim`. Accepted aliases include `subject`, `id`, and `evidence_kind`. Generic artifact intake registers provenance without inferring evidence.

## Canonical catalog document

Path:

```text
Catalog/catalog.tgcatalog.json
```

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
  "GovernanceHistory": []
}
```

Reload rejects mismatched workspace ID, profile ID, game version, or branch.

`GovernanceHistory` is optional when reading older schema-1 documents. New governance decisions append entries to it.

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
  "CreatedAt": "2026-07-17T12:00:00.000Z",
  "UpdatedAt": "2026-07-17T12:00:00.000Z",
  "SupersededByRecordId": ""
}
```

Required canonical fields:

- `RecordId`;
- `Domain`;
- `RecordKind`;
- `SubjectRef`;
- `IdentityKind`;
- at least one `EvidenceId`.

Identity rules:

- `native` requires `NativeRefExact` and no custom owner claim;
- `synthetic` requires an existing `OwnerPackId` and no borrowed native ref;
- `composite` and `source_scoped` require explicit reviewed meaning and evidence.

The database rejects duplicate record IDs and duplicate non-empty exact native references. Display names may repeat and are never identity keys.

Promotion creates `unvalidated`, staleness-`unknown` records and cannot populate `AllowedUsages`.

## Catalog relationship

```json
{
  "RelationshipId": "relationship.example.contains",
  "FromRecordId": "record.parent",
  "ToRecordId": "record.child",
  "TargetSubjectRef": "",
  "RelationshipKind": "contains",
  "EvidenceIds": ["evidence.fingerprint.2"],
  "ResearchStage": "S4",
  "Confidence": "documented",
  "OperationalRisk": "unknown",
  "ValidationState": "unvalidated",
  "StalenessState": "unknown",
  "AllowedUsages": [],
  "ForbiddenUsages": ["no_unvalidated_runtime_use"],
  "MissingRefs": [],
  "ConflictRefs": [],
  "Attributes": [],
  "CreatedAt": "2026-07-17T12:00:00.000Z",
  "UpdatedAt": "2026-07-17T12:00:00.000Z",
  "SupersededByRelationshipId": ""
}
```

A relationship requires a stable ID, existing source record, relationship kind, target record or unresolved target subject, and evidence. Relationships carry the same independent governance axes as records.

## State vocabularies

### Maturity/research stage

Preferred values: `S0` through `S8`. Legacy descriptive values remain readable for compatibility.

### Confidence

- `unknown`;
- `unrated`;
- `hypothesis`;
- `inferred`;
- `documented`;
- `runtime_observed`;
- `validated`.

### Operational risk

- `unknown`;
- `low`;
- `medium`;
- `high`;
- `critical`.

### Validation state

- `unvalidated`;
- `pending`;
- `validated`;
- `failed`;
- `stale`;
- `blocked`.

### Staleness state

- `unknown`;
- `current`;
- `potentially_stale`;
- `stale`.

Allowed usages require `ValidationState: validated`, `StalenessState: current`, no missing/conflict refs, and no supersession.

## Catalog validation event

```json
{
  "ValidationId": "validation.record.native-item-example.1",
  "SubjectKind": "record",
  "SubjectId": "native.item.example",
  "RecordId": "native.item.example",
  "State": "validated",
  "Method": "runtime-observation",
  "Validator": "validator-id",
  "CheckedAt": "2026-07-17T13:00:00.000Z",
  "ProfileId": "foa.mono.current",
  "GameVersion": "exact-version",
  "Branch": "mono",
  "EvidenceIds": ["evidence.fingerprint.3"],
  "Notes": "Observed in the configured build."
}
```

`SubjectKind` is `record` or `relationship`. `SubjectId` is the corresponding stable ID. `RecordId` remains for backward compatibility with older record-only events.

New validation events require state, method, named validator, exact profile/version/branch binding, and evidence IDs. History is retained separately from current state.

## Catalog governance event

```json
{
  "EventId": "governance.record.native-item-example.1",
  "SubjectKind": "record",
  "SubjectId": "native.item.example",
  "Axis": "permission",
  "PreviousValue": "unset",
  "NewValue": "allow",
  "Usage": "display_only",
  "EvidenceIds": ["evidence.fingerprint.3"],
  "ValidationIds": ["validation.record.native-item-example.1"],
  "Reviewer": "reviewer-id",
  "DecidedAt": "2026-07-17T13:05:00.000Z",
  "Notes": "Approved only for display in authoring tools."
}
```

Governance event axes currently include:

- `maturity`;
- `confidence`;
- `operational_risk`;
- `staleness`;
- `permission`;
- `supersession`.

Permission outcomes are `allow`, `forbid`, and `clear`. `allow` requires validation proof IDs with at least one `validated` event for the same subject.

Governance history is append-only through normal editor workflows. Current record or relationship fields store the latest state; history explains how the state was reached.

## Fail-closed transition rules

- validation never creates an allowed usage;
- non-validated states remove allowed usages;
- stale or potentially stale subjects lose allowed usages;
- superseded subjects lose allowed usages and become stale;
- returning to `current` does not restore old permissions;
- the same usage cannot be both allowed and forbidden;
- permission proof must belong to the same subject;
- relationship governance evidence must already be linked to the relationship.

## Compatibility and migration

Backward-compatible examples:

- adding an optional field with a safe default;
- adding `GovernanceHistory` to schema 1;
- adding subject-scoped validation fields while retaining legacy `RecordId`;
- adding a new issue or blocker code;
- adding a query filter;
- adding an importer that does not reinterpret existing documents.

Breaking examples:

- changing identity semantics;
- renaming or removing a field;
- changing a field type;
- changing fingerprint or profile-binding scope;
- changing exact-ref uniqueness;
- changing pack ownership requirements;
- changing a state value's meaning;
- merging previously distinct record kinds.

Breaking changes require a schema increment, migration or explicit unsupported-version error, old/new fixtures, tests, changelog and guide updates, release notes, and rollback guidance.
