# Quest inspection and authoring

## Decision and ownership
The owner explicitly requested a complete Quest and State Inspector followed by local quest authoring. This Significant slice extends historical contract-only gates for local authoring while preserving their engine-neutral data and no-execution rules. Primary owner is content-pack-authoring; Core validates, Foundation owns transactions and evidence, and Qt presents snapshots and sends commands.

## Contracts
QuestDefinition V1 and QuestBindingManifest V1 remain unchanged. Catalog schema 6 adds QuestProfiles attached to synthetic, pack-owned canonical records (domain narrative, kind quest; record ID equals quest ID). Each profile stores the canonical V1 definition JSON, display labels keyed to its exact internal IDs, state-key declarations and local logical-subject-to-catalog bindings. These bindings describe author intent and are not runtime-resolved QuestBindingManifests or permission grants.

State keys declare boolean, integer or text defaults; they do not expose or write current game state. Fact/decision/counter uses are type checked. Internal phase/objective/role references are resolved exactly. External actors, items, locations and quests use exact canonical records; missing or wrong-kind targets block saving. Unbound roles remain visible draft warnings until used by a condition requiring them. Imported V1 documents remain inspectable without a workspace; adopting a valid document into the active pack creates a new quest identity and preserves internal identities and inert payloads.

Bounds: 256 profiles and 8 MiB total definition JSON per catalog; 1 MiB per V1 document; 256 labels, state declarations and bindings per quest; existing V1 limits remain. Search and rendering use loaded snapshots without filesystem scans. Graph nodes represent phases, edges represent transitions; disconnected phases and repeat transitions are displayed without simulating execution. Graph rendering is capped by the V1 phase/transition bounds.

## Persistence and failure
Quest profiles were introduced in version 6. Current readers accept catalog versions 1–7; saves emit 7. Versions 1–5 with nonempty quest collections and future versions are rejected. Exact pre-migration backup is retained before schema-7 replacement. Downgrade requires the older build plus that backup. Other schemas, engine pins and runtime contracts are unchanged.

Every save verifies active compatible pack, exact owner and expected previous quest payload (optimistic conflict check), validates a candidate and creates author-intent evidence before durable catalog publication. Failed writes retain published state and UI drafts. Imported file reads are bounded and read-only; imported input never silently gains authority. Dirty selection/close/workspace/profile/pack transitions retain drafts or require explicit discard.

## Validation and performance
Require static/source-policy validation; exact pinned SDK Core, Framework, Editor and compiled tests; unchanged V1 contract tests plus quest authoring, malformed/reference/ownership/atomicity/migration tests; live Editor graph, search, all row edits, state bindings, imported inspection, save/reopen/failure, dirty guards and small-window evidence. Record maximum UI pause against the existing three-second local Editor budget. Generated artifacts and copied QA catalog data remain outside source. Game runtime, installer, deployment, native extraction and release lanes are not applicable.
