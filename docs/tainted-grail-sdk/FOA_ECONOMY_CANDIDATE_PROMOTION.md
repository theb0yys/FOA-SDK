# FoA Economy Candidate Promotion

## Purpose

`foa_economy_candidate_promotion.py` is the first domain-specific review workflow after the local identifier, collector, and source/evidence intake bridge.

It consumes the general catalog-promotion candidates emitted by `foa_game_data_intake.py` and stages a focused economy review document for item, recipe, and station candidates. It does not mutate the live catalog, does not create runtime permissions, and does not deploy content.

The intended chain is:

```text
managed identifier export
→ foa-identifiers.json
→ bounded local diagnostic collector
→ capture.foa-local-capture.json
→ source/evidence/catalog-candidate intake
→ economy candidate promotion staging
→ human review and later catalog promotion
```

## Inputs

The tool requires three existing documents:

```text
workspace.tgworkspace.json
evidence.tgevidence.json
<tgcatalog-candidates>.json
```

The candidate input must be a schema-1 `foa-catalog-promotion-candidates` document. The evidence document must bind to the same source ID and source fingerprint. Both documents must match the active workspace profile.

The tool accepts only economy records with these candidate kinds:

```text
item
recipe
station
crafting_station
interaction_target
```

Other domains or record kinds are rejected into the review document rather than silently promoted.

## Commands

### Stage an economy review document

```powershell
python Gems/TaintedGrailModdingSDK/Tools/foa_economy_candidate_promotion.py stage `
  --workspace path\to\workspace.tgworkspace.json `
  --candidates path\to\Catalog\Candidates\source-id.tgcatalog-candidates.json `
  --evidence path\to\Sources\source-id\evidence.tgevidence.json `
  --output path\to\EconomyPromotion\source-id.tgeconomy-promotion.json `
  --reviewer maintainer-id `
  --staged-at 2026-07-28T00:00:00Z
```

`--staged-at` is optional during ordinary use. Supplying it makes fixtures and tests deterministic.

### Verify a staged document

```powershell
python Gems/TaintedGrailModdingSDK/Tools/foa_economy_candidate_promotion.py verify `
  --workspace path\to\workspace.tgworkspace.json `
  --input path\to\source-id.tgeconomy-promotion.json
```

### Generate a project-owned fixture

```powershell
python Gems/TaintedGrailModdingSDK/Tools/foa_economy_candidate_promotion.py fixture `
  --output path\to\fixture-output
```

## Output document

The output document uses:

```json
{
  "SchemaVersion": 1,
  "DocumentKind": "foa-economy-candidate-promotion",
  "PromotionAllowed": false,
  "CatalogMutationAllowed": false,
  "RuntimePermissionGranted": false,
  "AdapterExecutionAllowed": false
}
```

It contains:

- `RecordPromotions` — one reviewed promotion row per staged candidate record.
- `EconomyItemProfiles` — draft item profile rows with unknown-safe defaults.
- `EconomyRecipeProfiles` — incomplete recipe profile drafts.
- `EconomyStationProfiles` — station profile review rows.
- `RejectedRecords` — candidates with record-specific errors.
- `Issues` — deterministic blocker and warning rows.

All generated records retain evidence IDs. Display names are preserved only as labels. They are not identity keys.

## Item candidates

Item candidates can produce draft `EconomyItemProfiles`.

The generated item profile uses unknown-safe defaults:

```text
Category: unknown
StackLimit: 0
Weight: 0
BaseValue: 0
Rarity: unknown
Quality: unknown
```

The profile is still a draft. It is not validated gameplay behavior and does not permit item grant, vendor injection, loot injection, reward injection, or custom item registration.

Every staged item retains `no_unvalidated_runtime_use`.

## Recipe candidates

Recipe candidates produce incomplete `EconomyRecipeProfiles`.

Recipe candidates remain incomplete until a later review supplies:

- recipe type;
- station records;
- output joins;
- ingredient joins, where applicable;
- unlock/learnability details;
- persistence-mode review;
- duplicate-key review.

The staged workflow intentionally adds a recipe warning because a recipe without station and output joins cannot become a runtime recipe append or custom registration candidate.

## Station candidates

Station, crafting-station, and interaction-target records are staged separately as station review rows.

Station review does not prove that a recipe is craftable or visible in game. It only gives the economy review workflow a stable station identity candidate for later recipe-profile completion.

## Blockers and duplicate handling

The workflow fails closed on or records blockers for:

- profile mismatch;
- source/evidence binding mismatch;
- missing evidence IDs;
- non-economy domains;
- unsupported economy record kinds;
- duplicate record IDs;
- duplicate native references;
- invalid native/synthetic ownership;
- candidate input issues;
- private or absolute path leakage;
- any attempted authority escalation.

Input candidate issues are preserved rather than overwritten.

## Authority boundary

This workflow is review-only.

It does not:

- mutate `Catalog/catalog.tgcatalog.json`;
- promote records into the live catalog;
- validate runtime behavior;
- grant `existing_item_grant`;
- grant `existing_recipe_learn`;
- grant `runtime_recipe_append`;
- grant `custom_item_registration`;
- grant `custom_recipe_registration`;
- build packages;
- deploy files;
- launch FoA;
- call Unity, BepInEx, Harmony, or FoA APIs;
- inspect or mutate saves.

A staged economy promotion document is evidence-bound review material. It is not a runtime deployment path and not a permission decision.

## Validation

Focused checks:

```powershell
python -m py_compile `
  Gems/TaintedGrailModdingSDK/Tools/foa_economy_candidate_promotion.py `
  Gems/TaintedGrailModdingSDK/Tools/validate_foa_economy_candidate_promotion.py

python -m unittest discover `
  -s Gems/TaintedGrailModdingSDK/Tools/tests `
  -p test_foa_economy_candidate_promotion.py `
  -v

python Gems/TaintedGrailModdingSDK/Tools/validate_foa_economy_candidate_promotion.py
```

Full acceptance still requires the normal repository validation, exact-head O3DE build evidence, compiled tests where applicable, and Windows UI evidence when the workflow is surfaced in the Editor.
