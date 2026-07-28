# FoA Local Diagnostic Collector

## Purpose

The bounded FoA local diagnostic collector creates the sanitized `foa-local-capture` JSON input used by the read-only game-data intake bridge.

It exists to close the first practical gap between a configured lawful FoA installation and the SDK source/evidence/catalog-candidate path:

```text
configured workspace game profile
→ bounded local diagnostic collector
→ capture.foa-local-capture.json
→ foa_game_data_intake.py capture
→ source/evidence documents and catalog-promotion candidates
```

## Commands

Run from the FOA-SDK checkout.

### Collect

```powershell
python Gems/TaintedGrailModdingSDK/Tools/foa_local_diagnostic_collector.py collect `
  --workspace C:\Path\To\workspace.tgworkspace.json `
  --output C:\Path\To\capture.foa-local-capture.json
```

For deterministic review fixtures, provide a whole-second UTC capture time:

```powershell
python Gems/TaintedGrailModdingSDK/Tools/foa_local_diagnostic_collector.py collect `
  --workspace C:\Path\To\workspace.tgworkspace.json `
  --output C:\Path\To\capture.foa-local-capture.json `
  --captured-at 2026-07-28T00:00:00Z
```

By default, the collector hashes only allowlisted top-level install marker files and allowlisted managed assembly names. To record path/layout observations without file hashes:

```powershell
python Gems/TaintedGrailModdingSDK/Tools/foa_local_diagnostic_collector.py collect `
  --workspace C:\Path\To\workspace.tgworkspace.json `
  --output C:\Path\To\capture.foa-local-capture.json `
  --no-file-hashes
```

### Verify

```powershell
python Gems/TaintedGrailModdingSDK/Tools/foa_local_diagnostic_collector.py verify `
  --workspace C:\Path\To\workspace.tgworkspace.json `
  --input C:\Path\To\capture.foa-local-capture.json
```

### Fixture

```powershell
python Gems/TaintedGrailModdingSDK/Tools/foa_local_diagnostic_collector.py fixture `
  --output C:\Path\To\collector-fixture
```

The fixture uses only project-owned synthetic files.

## Identifier exports

The collector can fold an explicit workspace-local identifier export into the local-capture JSON.

Default location:

```text
<ExtractedDataPath>/foa-identifiers.json
```

Explicit location:

```powershell
python Gems/TaintedGrailModdingSDK/Tools/foa_local_diagnostic_collector.py collect `
  --workspace C:\Path\To\workspace.tgworkspace.json `
  --identifier-export C:\Path\To\Workspace\Extracted\foa-identifiers.json `
  --output C:\Path\To\capture.foa-local-capture.json
```

Identifier exports must remain inside the active profile's `ExtractedDataPath`. They use this shape:

```json
{
  "SchemaVersion": 1,
  "ProfileId": "foa.mono.current",
  "GameVersion": "1.23.401",
  "Branch": "mono",
  "RuntimeTarget": "Mono",
  "PromoteAutomatically": false,
  "GrantsRuntimePermission": false,
  "Observations": [
    {
      "ObservationId": "observation.local.item.native-ref",
      "SubjectRef": "subject:foa:economy:item:iron-ore",
      "ClaimId": "native_ref_exact",
      "Claim": "Native item identifier was observed in a bounded local identifier export.",
      "Value": "00000000-0000-0000-0000-000000000001",
      "Domain": "economy",
      "RecordKind": "item",
      "IdentityKind": "native",
      "NativeRefExact": "00000000-0000-0000-0000-000000000001",
      "DisplayName": "Iron Ore",
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

The collector rewrites file locators through sanitized `$extracted/...` token paths. Absolute paths are rejected.

## Intake handoff

After collection, pass the generated capture to the existing intake tool:

```powershell
python Gems/TaintedGrailModdingSDK/Tools/foa_game_data_intake.py capture `
  --workspace C:\Path\To\workspace.tgworkspace.json `
  --input C:\Path\To\capture.foa-local-capture.json `
  --output C:\Path\To\Workspace\Extracted\Intake\capture-001
```

The intake output remains candidate evidence and catalog-promotion material. It is not a promoted catalog record and not runtime permission.

## Bounded collection model

The collector is deliberately narrow.

It may:

- read the active workspace document;
- validate that configured FoA install/profile paths exist and remain in their expected containment boundaries;
- check the configured Unity data root, managed assembly path, BepInEx plugin path, and workspace extracted-data path;
- hash only explicitly allowlisted top-level files, subject to file-size limits;
- read explicit identifier exports from `ExtractedDataPath`;
- write one sanitized local-capture JSON file.

It does not recursively scan, does not read arbitrary game directories, does not load Unity assemblies, does not execute BepInEx or Harmony, does not call FoA APIs, does not copy proprietary payloads, does not mutate game files or saves, does not promote catalog records, does not grant runtime permission, and does not package, deploy, launch, sign, or publish anything.

## Failure cases

Collection fails closed when:

- the active workspace profile is missing or ambiguous;
- the configured install path does not exist;
- `ManagedAssembliesPath` or `PluginPath` escape the configured install root;
- `ExtractedDataPath` escapes the workspace root;
- an identifier export is outside `ExtractedDataPath`;
- an identifier export does not match the exact active profile;
- a locator contains an absolute/private path;
- timestamps are not whole-second UTC;
- any capture or observation tries to set `PromoteAutomatically` or `GrantsRuntimePermission` to true.

## Review boundary

This collector is the producer for the existing intake bridge. It does not replace the catalog browser, source/evidence registry, governance engine, item/recipe editor, adapter capability matrix, work-order planner, package preview, or deployment result contracts.

A mod is still not made end-to-end until later slices add reviewed catalog promotion UI, target-specific adapter implementation, build/package generation, deployment preview execution, no-op load evidence, bounded runtime capability proof, and rollback/cleanup evidence.
