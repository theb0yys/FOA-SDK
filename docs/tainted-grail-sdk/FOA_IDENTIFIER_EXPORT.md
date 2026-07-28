# FoA Identifier Export Contract

## Purpose

`foa-identifiers.json` is the explicit handoff file for game-data identifiers that the SDK can ingest without scanning arbitrary game directories or granting runtime authority.

The file lives under the active workspace `ExtractedDataPath` and is consumed by `foa_local_diagnostic_collector.py`. The collector folds validated observations into `capture.foa-local-capture.json`; `foa_game_data_intake.py` then turns that capture into source, evidence, and catalog-promotion candidate documents.

```text
external lawful diagnostic observation
→ ExtractedDataPath/foa-identifiers.json
→ foa_local_diagnostic_collector.py collect
→ capture.foa-local-capture.json
→ foa_game_data_intake.py capture
→ source/evidence documents and catalog candidates
```

## Boundary

The identifier-export contract is an offline data contract. It does not scan the install tree, recursively inspect game folders, load Unity assemblies, execute BepInEx or Harmony code, call FoA APIs, copy proprietary assets, inspect saves, promote catalog facts, grant runtime permission, build packages, deploy, launch, sign, or publish.

A valid export proves only that a sanitized observation was supplied for the exact active workspace profile. The normal source/evidence, catalog, validation, governance, adapter, and review gates still apply.

## Commands

Verify an export against the active workspace profile:

```powershell
python Gems/TaintedGrailModdingSDK/Tools/foa_identifier_export.py verify `
  --workspace C:\Path\To\workspace.tgworkspace.json `
  --input C:\Path\To\Workspace\Extracted\foa-identifiers.json
```

Normalize an export to deterministic JSON:

```powershell
python Gems/TaintedGrailModdingSDK/Tools/foa_identifier_export.py normalize `
  --workspace C:\Path\To\workspace.tgworkspace.json `
  --input C:\Path\To\Workspace\Extracted\draft-identifiers.json `
  --output C:\Path\To\Workspace\Extracted\foa-identifiers.json
```

Generate a synthetic project-owned fixture:

```powershell
python Gems/TaintedGrailModdingSDK/Tools/foa_identifier_export.py fixture `
  --output C:\Path\To\Temp\foa-identifier-export-fixture
```

## Document shape

```json
{
  "SchemaVersion": 1,
  "DocumentKind": "foa-identifier-export",
  "ExportId": "export.foa.local.identifiers",
  "ProfileId": "foa.mono.current",
  "GameVersion": "1.23.401",
  "Branch": "mono",
  "RuntimeTarget": "Mono",
  "ToolName": "FoA Diagnostic Exporter",
  "ToolVersion": "1.0.0",
  "CapturedAt": "2026-07-28T00:00:00Z",
  "PromoteAutomatically": false,
  "GrantsRuntimePermission": false,
  "Observations": []
}
```

`ProfileId`, `GameVersion`, `Branch`, and `RuntimeTarget` must match the active workspace profile exactly. Timestamps use whole-second UTC. `PromoteAutomatically` and `GrantsRuntimePermission` must remain false at the document and observation levels.

## Observation shape

```json
{
  "ObservationId": "observation.example.item.native-ref",
  "SubjectRef": "subject:foa:economy:item:example",
  "ClaimId": "native_ref_exact",
  "Claim": "Native item identifier was observed in a sanitized diagnostic export.",
  "Value": "00000000-0000-0000-0000-000000000001",
  "Domain": "economy",
  "RecordKind": "item",
  "IdentityKind": "native",
  "NativeRefExact": "00000000-0000-0000-0000-000000000001",
  "DisplayName": "Example Item",
  "EvidenceKind": "native-identifier-observation",
  "Confidence": "observed",
  "Locator": "$.items[0].guid",
  "RecordPath": "$.Observations[0]",
  "PromoteAutomatically": false,
  "GrantsRuntimePermission": false
}
```

Supported `ClaimId` values are:

- `native_ref_exact`
- `unity_guid`
- `addressable_key`
- `assetbundle_name`
- `managed_type_name`
- `template_key`
- `recipe_key`
- `localization_key`
- `asset_path_token`
- `diagnostic_fact`

Supported domains are `economy`, `population`, `world`, `quest`, `dialogue`, `audio`, `ui`, and `runtime`. `Domain` and `RecordKind` must be supplied together. `native` observations cannot claim an owner pack; `synthetic` observations require `OwnerPackId` and cannot carry `NativeRefExact`.

## Private path and payload handling

Observation values, locators, display names, and claims must not contain absolute/private paths such as `C:\...`, UNC paths, `/home/...`, or `~/...`. Locators must use JSON paths or sanitized token locators such as `$extracted/...`.

The export records identifiers and metadata only. It must not embed proprietary game assets, decompiled source, assembly bytes, save data, credentials, or private machine paths.

## Validation behaviour

The validator rejects:

- profile mismatches;
- unknown schema versions;
- missing or malformed observation IDs;
- duplicate observation IDs;
- duplicate `NativeRefExact` values;
- malformed GUIDs for `native_ref_exact` and `unity_guid` claims;
- unsupported domains, identity kinds, evidence kinds, confidence values, or claim IDs;
- synthetic records without owner packs;
- native records with owner packs;
- `PromoteAutomatically` or `GrantsRuntimePermission` set to true;
- files outside the active profile `ExtractedDataPath` when `--workspace` is supplied.

## Relationship to the collector and intake tools

`foa_identifier_export.py` validates and normalizes the export file. `foa_local_diagnostic_collector.py` reads the export from `ExtractedDataPath`, folds those observations into the local-capture document, and preserves the same false-authority boundary. `foa_game_data_intake.py` consumes that capture and creates the source/evidence/catalog-candidate output for human review.

This keeps the data path functional while preserving the runtime boundary.
