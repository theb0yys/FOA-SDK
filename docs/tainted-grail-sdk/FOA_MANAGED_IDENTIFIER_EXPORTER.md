# FoA Managed Identifier Exporter

## Purpose

`foa_managed_identifier_exporter.py` is the first producer for the
`foa-identifiers.json` contract.

It turns bounded local managed-metadata observations into the deterministic
identifier export consumed by:

```text
foa_managed_identifier_exporter.py export
→ ExtractedDataPath/foa-identifiers.json
→ foa_identifier_export.py verify/normalize
→ foa_local_diagnostic_collector.py collect
→ capture.foa-local-capture.json
→ foa_game_data_intake.py capture
→ source/evidence documents and catalog-promotion candidates
```

This is still a diagnostic bridge, not a runtime adapter and not a mod
deployment path.

## Commands

Run from the FOA-SDK checkout.

```powershell
python Gems/TaintedGrailModdingSDK/Tools/foa_managed_identifier_exporter.py export `
  --workspace <workspace.tgworkspace.json> `
  --captured-at 2026-07-28T00:00:00Z `
  --replace
```

By default the exporter writes:

```text
<ExtractedDataPath>/foa-identifiers.json
```

The output may be checked independently:

```powershell
python Gems/TaintedGrailModdingSDK/Tools/foa_managed_identifier_exporter.py verify `
  --workspace <workspace.tgworkspace.json> `
  --input <ExtractedDataPath>/foa-identifiers.json
```

A synthetic project-owned fixture is available:

```powershell
python Gems/TaintedGrailModdingSDK/Tools/foa_managed_identifier_exporter.py fixture `
  --output <temporary-output>
```

## Inputs

The exporter uses the active profile from the workspace document.

Required profile fields:

- `InstallPath`
- `ManagedAssembliesPath`
- `ExtractedDataPath`
- `ProfileId`
- `GameVersion`
- `Branch`
- `RuntimeTarget`

The managed path must remain inside the configured install path. The extracted
data path must remain inside the workspace root.

## Managed assembly observation

The first bounded assembly source is allowlisted to:

```text
Assembly-CSharp.dll
```

The exporter reads bounded bytes from that file and extracts candidate
managed-type-like strings. It does not load assemblies as code, does not decompile
methods, does not run Unity, FoA, BepInEx, or Harmony, and does not
execute any runtime code.

Observed managed-type-like strings become `managed_type_name` observations with
`managed-type-observation` evidence and `inferred` confidence.

## Optional seed observations

A project-owned seed file may be placed at:

```text
<ExtractedDataPath>/foa-managed-identifier-seeds.json
```

or passed explicitly with `--seed`, but every seed must remain inside
`ExtractedDataPath`.

Supported seed collections are:

```json
{
  "ManagedTypes": ["Game.Inventory.InventoryService"],
  "TemplateKeys": [
    {
      "Value": "Characters/Templates/Bandit",
      "SubjectRef": "subject:foa:population:template:bandit",
      "DisplayName": "Bandit Template"
    }
  ],
  "RecipeKeys": [
    {
      "Value": "Crafting/Recipes/IronIngot",
      "SubjectRef": "subject:foa:economy:recipe:iron-ingot",
      "DisplayName": "Iron Ingot Recipe"
    }
  ],
  "AddressableKeys": [],
  "NativeRefs": []
}
```

The seed file may also contain an `Observations` array using the already-reviewed
`foa-identifiers.json` observation shape. Seed observations are revalidated by
`foa_identifier_export.py`.

Required seed header:

```json
{
  "SchemaVersion": 1,
  "ProfileId": "foa.mono.current",
  "GameVersion": "1.23.401",
  "Branch": "mono",
  "RuntimeTarget": "Mono",
  "PromoteAutomatically": false,
  "GrantsRuntimePermission": false
}
```

## Output

The exporter writes a normalized `foa-identifiers.json` document using the
contract from `FOA_IDENTIFIER_EXPORT.md`.

The generated document keeps:

```json
{
  "PromoteAutomatically": false,
  "GrantsRuntimePermission": false
}
```

No output grants catalog promotion, adapter permission, runtime permission,
deployment permission, or save access.

## Boundary

This tool does not:

- recursively scan the game install;
- load assemblies as code;
- decompile proprietary source;
- run Unity;
- run FoA;
- run BepInEx;
- run Harmony;
- call FoA APIs;
- copy game payloads into the repository;
- inspect saves;
- mutate game files;
- promote catalog facts;
- grant runtime permission;
- build packages;
- deploy, launch, sign, or publish.

It produces candidate observations only.

## Validation

Focused validation:

```powershell
python -m py_compile `
  Gems/TaintedGrailModdingSDK/Tools/foa_managed_identifier_exporter.py `
  Gems/TaintedGrailModdingSDK/Tools/validate_foa_managed_identifier_exporter.py

python -m unittest discover `
  -s Gems/TaintedGrailModdingSDK/Tools/tests `
  -p test_foa_managed_identifier_exporter.py `
  -v

python Gems/TaintedGrailModdingSDK/Tools/validate_foa_managed_identifier_exporter.py
```

Full exact-head acceptance still requires the normal repository validation,
O3DE configure/build, compiled tests, and Windows evidence gates where
applicable.

## Next development step

After this exporter, the next planned development unit is the
economy/item/recipe candidate promotion workflow.

That workflow should consume catalog-promotion candidates produced by the intake
pipeline and provide a domain-specific review path for native item, recipe,
station, ingredient, output, and acquisition relationship records.
