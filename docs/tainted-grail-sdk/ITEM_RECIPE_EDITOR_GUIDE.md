# Item and Recipe Editor Guide

## Purpose

The Item and Recipe Editor is the first specialised domain authoring tool built on the shared TG SDK foundation.

Open it from:

**Tools → Tainted Grail SDK → Tainted Grail Item and Recipe Editor**

The tool authors typed economy data for canonical catalog records. It does not create game identities, call FoA APIs, register runtime templates, grant inventory items, learn recipes, append recipes, mutate vendors or loot, deploy files, or modify saves.

## Core separation

**An item record is not a recipe record.**

The editor keeps these concepts independent:

- an **item record** identifies a game-owned or pack-owned item;
- a **recipe record** identifies a game-owned, runtime-appended, or pack-owned recipe definition;
- an **ingredient join** describes one required input for a recipe;
- an **output join** describes one output or by-product from a recipe;
- a **station record** identifies a crafting or interaction context;
- an **acquisition relationship** describes where an item or recipe may be sold, dropped, found, rewarded, learned, granted, or crafted;
- a **permission decision** authorises one named downstream usage lane after validation.

A display name never joins these records. Every profile and join uses stable canonical record IDs or an explicit unresolved subject reference.

## Start working

The pane opens the saved workspace automatically. Use **Load game items and
recipes** to read the registered installation. The reader runs in a separate
process and the same button cancels it. Definitions are saved locally with their
source evidence; repeated loading preserves existing local profiles, joins and
governance. Type part of a name in a selection box, then choose a matching result.

To create your own definitions, use **Choose or create mod**, enter a name and
author in Pack Manager, and **Save mod**. Return to this pane and choose **New
item** or **New recipe**. Foundation assigns a new pack-owned identity and records
the authoring intent as a local source. A custom identity never borrows a native
game identity. Use **Save Item Profile** or **Save Recipe Profile** after editing.

For recipes, select an ingredient or output row to load its fields. Change the
item or quantity and use **Add / Update** to save. **New ingredient** and **New
output** clear the form for another link; link IDs are assigned automatically.
**Remove selected** removes only that recipe's selected link and saves the change.
Each link save/removal is a separate catalog transaction. A recipe without an
output can be saved as incomplete authoring work.

Unsaved form values remain in this pane while switching definitions or receiving
Foundation updates. Closing a docked or floating pane with unsaved changes offers
**Save / Discard / Cancel**. Save includes retained drafts for other definitions:
item and recipe profiles, ingredient and output forms, and acquisition fields.
Cancel, Escape, or dismissing the prompt keeps the pane open. A failed save also
keeps it open with the remaining drafts and an error to correct. Each form saves
separately: earlier successful saves remain saved if a later form fails. Retry
Save after correcting the error. Discard closes without saving remaining drafts.
A clean form or one reverted to its saved values closes without prompting.

Opening a workspace from SDK Status or Catalog Browser offers the same
**Save / Discard / Cancel** choices for all retained drafts. Save writes to the
current workspace before switching. Cancel or a failed Save keeps the current
workspace and remaining drafts. Discard clears drafts only after the replacement
succeeds; a later cancellation in Pack Manager also preserves them. Invalid
workspace files leave the current forms intact.

Reloading the same workspace uses the same protection. A successful Save reloads
the newly saved profiles, links and evidence. A successful replacement resets the
forms, so drafts cannot appear in another workspace with matching record IDs.
Crash recovery for this editor remains separate work. Saved definitions and links
use the canonical catalog's supported schema and migration path.

Acquisition relationships and evidence/permission details have separate tabs.
They still require exact associated evidence and do not grant runtime access.

## Installed-data coverage

### Preview an item or recipe

Select an item, then open **Visual Preview → Game icon**. Its installed game icon
loads automatically from the validated local preview cache. Opening Asset Browser
first is unnecessary. Selecting a recipe opens its first resolved output icon;
**Recipe-linked item** also lets you inspect its ingredients and other outputs.

**Refresh assets** rebuilds the local icon cache through the existing read-only
game reader. The button cancels an active refresh and preserves previous previews.
An item without a supported icon displays an explanation and clears the previous
image. Local items without a native reference use **Custom visuals** for asset
selection. That tab retains the processed O3DE preview and explicit binding
controls. Looking at a game icon never changes an item, recipe or asset binding.

### Reader coverage

The pinned reader supports serialized item fields and recipe ingredients,
quantities and outputs in the item/crafting bundles. It joins Addressables GUIDs
only through an exact, unambiguous container path, including abstract ingredient
templates. Missing or ambiguous references remain explicit unresolved subjects.
Bundle entries without the supported component fields are recorded as unsupported
in the private observation document.

Category labels are presentation groups. Stack limits, crafting stations, unlock
semantics and other fields absent from the supported source are not inferred.
The resulting records start at research stage S1 without runtime permissions.
Reading metadata, editing a definition or saving a catalog does not establish
that the game accepts that definition. Deployment and runtime validation remain
separate governed services.

The versioned `foa-native-economy-observations` document is local provider output,
not canonical interchange. Foundation verifies profile and source hashes, builds
and validates a complete catalog candidate, persists source evidence, then
publishes the catalog in one transaction. Failed catalog persistence preserves
the prior catalog and may leave unused source evidence for inspection. Generated
observations and game-derived content must remain outside the source checkout.

The worker retains the installed-item reader's 180-second deadline and source
limits. Economy intake accepts at most 10,000 definitions, 256 links per recipe
collection and a 16 MiB observation document. Quantities must be whole numbers
from 1 to 1,000,000. A failed save can be retried by loading definitions again;
each reader run supplies fresh observation and evidence identities.

## Supported canonical record kinds

### Items

Typed item profiles require:

```text
Domain: economy
RecordKind: item
```

### Recipes

Typed recipe profiles require:

```text
Domain: economy
RecordKind: recipe
```

### Stations

Recipe station IDs may reference canonical economy records using one of these kinds:

```text
station
crafting_station
interaction_target
```

Other economy records—such as vendors, loot sources, locations, quests, contracts, or rewards—may be targets of acquisition relationships when their canonical identities already exist.

## Existing-content and custom-content lanes

Native and synthetic identities remain separate.

### Existing native item

A native item record:

- preserves its exact native reference;
- has no custom pack owner;
- may eventually receive reviewed permissions such as `existing_item_grant` or `existing_item_consume`;
- cannot use `custom_item_registration`.

### Custom item

A synthetic item record:

- is owned by an existing pack ID;
- does not borrow a native exact reference;
- may eventually receive `custom_item_registration` after adapter proof exists;
- cannot use native-only grant or consume lanes before registration and identity resolution.

### Existing native recipe

A native recipe record:

- preserves its exact native reference;
- may use `native_template` persistence description;
- may eventually receive `existing_recipe_learn` after station visibility and learnability proof;
- cannot use `custom_recipe_registration`.

### Runtime-appended recipe

A reviewed recipe may use the descriptive persistence mode `runtime_append` when research shows that an adapter can safely append a recipe definition. This label does not grant permission. The separate `runtime_recipe_append` lane must still pass governance and adapter validation.

### Custom recipe

A synthetic recipe:

- is pack-owned;
- may use `custom_template` persistence description;
- may eventually receive `custom_recipe_registration`;
- cannot claim the existing native recipe-learn lane.

## Item profile

Select a canonical item record, then enter only values supported by evidence.

Fields include:

- category and subtype;
- stack limit, where `0` means unknown rather than unlimited;
- weight and base value;
- rarity and quality;
- durability, where `0` may represent unknown when the underlying item does not expose a durability value;
- quest/story-sensitive flag;
- unique flag;
- hidden flag;
- localisation name and description references;
- icon and asset references;
- tags;
- evidence IDs.

Negative weight, value, or durability is rejected.

### Quest and unique items

Marking an item as quest-sensitive or unique does not automatically forbid every use, but the blocker service treats broad distribution lanes as high-risk.

Quest-sensitive items are blocked from ordinary grant, consume, vendor/loot, and reward lanes until a dedicated quest-safety review exists.

Unique items are blocked from vendor/loot and reward distribution until uniqueness, persistence, cleanup, and rollback behavior are proven.

## Recipe profile

Select a canonical recipe record and provide:

- recipe type;
- recipe tab or UI grouping;
- one or more canonical station record IDs;
- unlock mode;
- unlock subject references;
- stable duplicate key;
- persistence mode;
- hidden flag;
- evidence IDs.

Supported persistence descriptions are:

- `unknown`;
- `native_template`;
- `runtime_append`;
- `custom_template`.

These values describe the researched implementation shape. They are not runtime permissions.

Native recipes cannot use `custom_template`. Synthetic recipes cannot claim `native_template`.

A recipe without a station or typed output is persisted only as incomplete research and remains blocked from recipe action lanes.

## Ingredient joins

Ingredients are typed join records rather than strings embedded in a recipe.

Each ingredient requires:

- stable link ID;
- recipe record ID;
- exactly one resolved item record ID or unresolved item subject reference;
- positive quantity;
- optional alternative group;
- consumed flag;
- optional conditions;
- evidence IDs.

Use an unresolved subject reference when the source proves an ingredient subject but canonical identity reconciliation is incomplete. Unresolved ingredients remain blocked for runtime recipe append and custom registration.

Alternative groups may represent substitutions or “one of” ingredient requirements. The group semantics must be documented in conditions and evidence; the editor does not infer them.

## Output and by-product joins

Each output requires:

- stable link ID;
- recipe record ID;
- exactly one resolved item record ID or unresolved item subject reference;
- positive quantity;
- probability greater than `0` and no greater than `1`;
- by-product flag;
- optional conditions;
- evidence IDs.

A recipe requires at least one typed output before recipe action lanes can be considered.

Unresolved outputs are more restrictive than unresolved ingredients because the resulting identity cannot be safely granted, registered, displayed, or persisted.

## Acquisition relationships

The shared relationship form creates first-class catalog relationships. Supported relationship kinds are:

- `sold_by`;
- `dropped_by`;
- `found_at`;
- `rewarded_by`;
- `learned_from`;
- `granted_by`;
- `crafted_at`.

Each relationship requires:

- stable relationship ID;
- source item or recipe record;
- exactly one target record ID or unresolved target subject reference;
- relationship kind;
- evidence IDs;
- optional attributes.

New relationships begin with:

```text
ResearchStage: S2
Confidence: inferred
OperationalRisk: unknown
ValidationState: unvalidated
StalenessState: unknown
ForbiddenUsages: no_unvalidated_runtime_use
```

The relationship must be reviewed in Catalog Governance before it can support planning or adapter work.

## Action-lane matrix

The Item and Recipe Editor displays action lanes as read-only status. It never changes them.

Possible status values are:

- `allowed` — a reviewed governance decision currently permits the lane;
- `forbidden` — the lane is explicitly prohibited;
- `unset` — no permission decision exists.

### Item lanes

- `existing_item_grant`;
- `existing_item_consume`;
- `custom_item_registration`;
- `asset_localisation_injection`;
- `vendor_or_loot_injection`;
- `quest_or_contract_reward_injection`.

### Recipe lanes

- `existing_recipe_learn`;
- `runtime_recipe_append`;
- `custom_recipe_registration`;
- `asset_localisation_injection`;
- `quest_or_contract_reward_injection`.

Use Catalog Governance to review validation and permissions. A green or `allowed` lane remains an editor-side decision only; a runtime adapter must independently confirm capability, compatibility, persistence, cleanup, and rollback before execution.

## Evidence rules

Item and recipe profile evidence must exist in the active evidence registry and match the canonical record’s subject reference.

Ingredient and output evidence must bind the exact link subject:
`economy-recipe-ingredient:<link ID>` or `economy-recipe-output:<link ID>`.
The pane records local authoring intent for that exact link and its values on
each save. Installed links receive separate observations from the reader.

Missing or unrelated evidence is rejected before persistence.

Acquisition relationships require evidence IDs and receive blockers when referenced evidence is missing or belongs outside the active source set.

## Durable storage

Typed economy data is stored in the same canonical document as identities, relationships, validation, and governance history:

```text
Catalog/catalog.tgcatalog.json
```

The schema-2 document preserves these economy collections from schema 1:

- `EconomyItems`;
- `EconomyRecipes`;
- `RecipeIngredients`;
- `RecipeOutputs`.

The complete candidate catalog is saved before the in-memory catalog is published. A failed validation or write leaves the current catalog unchanged.

## Blockers

Economy blockers include:

- canonical item or recipe without a typed profile;
- profile evidence missing or bound to another subject;
- recipe without a station;
- recipe without an output;
- unresolved ingredient or output identity;
- missing join evidence;
- identity-incompatible action lane;
- quest-sensitive item approved for ordinary distribution;
- unique item approved for vendor, loot, or reward distribution;
- native recipe configured as custom template;
- synthetic recipe configured as native template.

Resolve the underlying identity, evidence, profile, join, governance, or adapter proof. Do not remove a blocker merely to make coverage appear complete.

## What this tool does not do

The Item and Recipe Editor does not:

- scan arbitrary files outside the registered, supported item/recipe sources;
- fabricate missing item or recipe identities;
- parse opaque runtime data;
- grant or remove inventory items;
- learn or forget recipes;
- append recipes to station lists;
- register custom item or recipe templates;
- load Unity assets;
- inject localisation;
- mutate vendors, loot, rewards, quests, contracts, or saves;
- launch FoA;
- execute rollback.

Those actions belong to separately implemented and validated adapter, build, deployment, and test layers.

## Recommended review sequence

1. Verify the canonical identity and exact native reference or pack owner.
2. Verify profile evidence against the exact active game build.
3. Complete item or recipe profile fields without guessing.
4. Resolve stations, ingredients, outputs, and acquisition relationships.
5. Review economy blockers in Foundation Status.
6. Record maturity, confidence, risk, validation, and staleness in Catalog Governance.
7. Grant only the narrow usage lane supported by proof.
8. Keep runtime adapter execution outside the editor.

## Pane-close acceptance

Run `Tools/editor_tests/run_item_recipe_close_smoke.ps1` from the SDK Gem with
`-EditorExecutable`, `-EngineRoot`, `-CacheRoot` and a fresh external `-OutputRoot`.
The runner uses the exact pinned built Editor on an inactive private Windows
desktop, creates synthetic authoring records, and checks actual docked/floating
close controls, dirty/reverted forms, all retained drafts, partial saves, retry
after validation failure and a real catalog file-lock failure. It records loaded
module identity, individual checks and native exit status outside source.

The runner requires prepared Editor assets; compilation and static tests remain
separate gates. This acceptance establishes Editor authoring behavior only.

## Workspace-switch acceptance

Run the same private Editor runner with `-Suite workspace-status` and then
`-Suite workspace-catalog`, using a fresh external output root for each run.
These suites use the real workspace pickers and verify all retained forms,
Save/Discard/Cancel, picker cancellation, invalid targets, failed validation and
catalog writes, partial Save and retry, same-root reload freshness, a later Pack
Manager veto, failed post-admission reload and matching record IDs across roots.
Each workspace interaction must complete within the five-second synthetic fixture
budget. Run the default close suite as a separate regression check.
