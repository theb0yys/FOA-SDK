# Read-only FoA game-data intake

## Purpose

FoA game-data intake is the first functional bridge from an operator-provided FoA data observation into the SDK's existing source, evidence, and catalog-candidate path.

The goal is narrow: native IDs enter as candidate bindings that can be reviewed and promoted through the normal catalog workflow. The tool does not create a mod by itself and does not bypass catalog governance.

## What it does

`Gems/TaintedGrailModdingSDK/Tools/foa_game_data_intake.py` accepts two explicit inputs:

1. a saved `*.tgworkspace.json` document with one active FoA game profile;
2. a sanitized local-capture JSON document produced from lawful local observation or a separately reviewed diagnostic source.

It writes a contained intake output:

```text
Sources/<source-id>/source.tgsource.json
Sources/<source-id>/evidence.tgevidence.json
Catalog/Candidates/<source-id>.tgcatalog-candidates.json
source-input.foa-local-capture.json
foa-game-data-intake.manifest.json
```

The source and evidence documents use the existing SDK intake model. The catalog-candidate document is review input only: it lists candidate records and native bindings without mutating `Catalog/catalog.tgcatalog.json`.

## What it does not do

This slice deliberately does not:

- scan arbitrary disks or auto-discover game installs;
- extract copyrighted assets;
- copy game files into the repository;
- load Unity assemblies;
- execute BepInEx or Harmony;
- call Unity or FoA APIs;
- mutate game files, saves, workspaces, packs, or the published catalog;
- promote catalog records;
- validate gameplay behavior;
- grant runtime permission;
- build, package, deploy, launch, sign, or publish anything.

## Input shape

The local-capture input is JSON with `SchemaVersion: 1` and an exact binding to the active game profile:

```json
{
  "SchemaVersion": 1,
  "CaptureId": "capture.foa.local.example",
  "Title": "FoA local item identifier capture",
  "SourceKind": "foa-local-diagnostic-capture",
  "ProfileId": "foa.mono.current",
  "GameVersion": "1.23.401",
  "Branch": "mono",
  "RuntimeTarget": "Mono",
  "ToolName": "FoA Diagnostic Capture",
  "ToolVersion": "1.0.0",
  "CapturedAt": "2026-07-28T00:00:00Z",
  "Locator": "capture.foa-local-capture.json",
  "PromoteAutomatically": false,
  "GrantsRuntimePermission": false,
  "Observations": [
    {
      "ObservationId": "observation.example.item.native-ref",
      "SubjectRef": "subject:foa:economy:item:example",
      "ClaimId": "native_ref_exact",
      "Claim": "Native item identifier was observed in a sanitized local capture.",
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
  ]
}
```

Display names are carried only as review labels. They are never identity keys. Stable identity is carried through `SubjectRef`, exact native references, source fingerprints, observation IDs, and generated evidence IDs.

## Commands

Generate intake output from explicit inputs:

```powershell
python Gems/TaintedGrailModdingSDK/Tools/foa_game_data_intake.py capture `
  --workspace path\to\workspace.tgworkspace.json `
  --input path\to\capture.foa-local-capture.json `
  --output path\to\intake-output
```

For deterministic test evidence, pass an exact import time:

```powershell
python Gems/TaintedGrailModdingSDK/Tools/foa_game_data_intake.py capture `
  --workspace path\to\workspace.tgworkspace.json `
  --input path\to\capture.foa-local-capture.json `
  --output path\to\intake-output `
  --imported-at 2026-07-28T00:00:01Z
```

Generate a project-owned synthetic fixture:

```powershell
python Gems/TaintedGrailModdingSDK/Tools/foa_game_data_intake.py fixture `
  --output path\to\fixture-output
```

Verify generated output:

```powershell
python Gems/TaintedGrailModdingSDK/Tools/foa_game_data_intake.py verify `
  --output path\to\intake-output
```

## Review process

The intended process is:

```text
lawful local observation or reviewed diagnostic export
→ foa_game_data_intake.py capture
→ source/evidence documents
→ catalog-candidate records and native bindings
→ human review
→ normal catalog promotion
→ domain editors consume resolved canonical records
```

The candidate document keeps `PromotionAllowed: false` and `RuntimePermissionGranted: false`. Promotion remains a separate catalog operation. Runtime use remains a separate adapter/governance path.

## Failure model

The tool fails closed when:

- the capture profile does not match the active workspace profile;
- timestamps are not whole-second UTC;
- observation IDs are duplicated;
- a synthetic record borrows a native reference;
- a native record claims a pack owner;
- an observation requests automatic promotion or runtime permission;
- a candidate record duplicates a native reference;
- generated payload hashes do not match the manifest;
- candidate records reference missing evidence.

Duplicate candidate native references are preserved as blocking issues instead of being silently merged. The user must resolve the identity conflict before promotion.
