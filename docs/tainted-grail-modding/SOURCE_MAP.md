# Upstream Source Map

## Baseline

All initial rows are bound to `theb0yys/Tainted-Grail-The-Fall-of-Avalon-mods` commit `d7e740e7f167b73152b53409e483dab07d80d048`.

The file-level and repository-wide disposition inventory is now recorded in [`FILE_INVENTORY.md`](FILE_INVENTORY.md).

Status values:

- `existing-owner` — FOA-SDK already has the canonical result;
- `adapted` — rewritten for the current SDK and handbook;
- `extracted` — structured facts, patterns, or records have been extracted;
- `planned-adapt` — rewrite remains queued;
- `planned-extract` — extraction remains queued;
- `research-only` — preserve and investigate before publication;
- `blocked-payload` — do not copy the payload;
- `superseded` — historical workflow is replaced by the current SDK contract.

## Core documentation and process

| Upstream source | Subject | Destination or owner | Decision | Status |
|---|---|---|---|---|
| `docs/INDEX.md` | Ordered onboarding and governance read path | handbook root and indexes | adapt | `adapted` |
| `docs/foa-modding-environment.md` | Mono/BepInEx environment baseline and local-reference rules | `getting-started/`, `runtime/` | adapt and reverify | `adapted` |
| `docs/repo-layout.md` | Source repository organization and prohibited payloads | handbook process plus current FOA-SDK product layout | adapt | `planned-adapt` |
| `docs/research/README.md` and taxonomy | Research classes, metadata, placement, and promotion | `Research/` and handbook process | link/adapt | `planned-adapt` |
| `docs/engineering-process.md` | Design, implementation, review, and evidence process | `process/` | adapt | `planned-adapt` |
| `docs/code-review-standard.md` | Findings-first code review | current review/merge policy plus mod-specific review guide | link/adapt | `planned-adapt` |
| `docs/in-game-ui-quality-standard.md` | Player-facing UI quality | UI system reference and example acceptance | adapt | `planned-adapt` |
| `docs/codex-agent-system.md`, `docs/codex-workflow.md`, `codex/` | Repository-specific AI workflow | contributor guidance, not public mod runtime guidance | extract useful process only | `planned-extract` |
| `docs/compatibility-and-versioning.md` | Compatibility declarations and version scope | `reference/`, version profiles, package manifests | adapt | `adapted` |
| `docs/validation-matrix.md` | Evidence levels and validation claims | current SDK exact-head and runtime evidence model | map/supersede | `superseded` |
| `docs/gate-promotion-policy.md` | Promotion and runtime permission separation | governance engine and evidence guides | link | `existing-owner` |
| `docs/debugging-and-support.md` | Logs, triage, reproduction, support | `reference/` troubleshooting and `SUPPORT.md` | adapt | `planned-adapt` |
| `docs/diagnostic-tool-development-policy.md` | Safe diagnostics and privacy | SDK diagnostics/privacy policies | link/adapt | `planned-adapt` |
| `docs/decision-records.md` | Architectural decision recording | contributor process | adapt | `planned-adapt` |
| `docs/mod-lifecycle.md` | Research-to-support lifecycle | `process/` and first-mod path | adapt | `adapted` |
| `docs/release-checklist.md` | Build/package/release evidence | installer/release docs and mod release guide | adapt | `planned-adapt` |
| `docs/ecosystem/` | Identity, rename, ownership, and ecosystem controls | `reference/` and package/plugin identity guidance | adapt | `planned-adapt` |

## Game knowledge

| Upstream source | Subject | Destination or owner | Decision | Status |
|---|---|---|---|---|
| `docs/research/game-knowledge/README.md` | Full-game research governance | canonical catalog/research documentation | adapt/link | `existing-owner` |
| `RESEARCH-MAP.md` and `MASTER-RESEARCH-REGISTER.md` | Programme tiers and workstreams | handbook research backlog plus `Research/` | reconcile | `planned-adapt` |
| `MATURITY-AND-PROMOTION.md` | Research, validation, permission, and staleness axes | governance engine and handbook metadata | link/adapt | `existing-owner` |
| `IDENTIFIER-STANDARD.md` | Project IDs, native refs, aliases, synthetic IDs | canonical catalog/data-format docs | link | `existing-owner` |
| `ENTITY-RELATIONSHIP-MODEL.md` | Entity graph | canonical catalog and system reference | link/render | `existing-owner` |
| `CORPUS-MIGRATION-PLAN.md` | Existing encyclopaedia/report migration | handbook migration programme | reconcile | `planned-adapt` |
| `SCHEMA-GAP-ANALYSIS.md` | Source model versus target contracts | `Research/` | update from current schemas | `research-only` |
| `CONTENT-ADDITION-WORKFLOW.md` | Item, recipe, troop, encounter, and state additions | system-specific authoring guides | adapt | `planned-adapt` |
| `domains/` | World, actors, societies, economy, spawns, narrative, framework | `systems/` indexes linked to catalog records | extract/reconcile | `planned-extract` |
| `templates/` and `registers/` | Research note and register shapes | current research contracts | reconcile | `planned-extract` |
| `scopes/` | Cross-domain regional slices | handbook/system navigation only | link | `planned-extract` |
| `Research/tainedencyclapidia.md` and dated reports | Human-readable corpus | preserved `Research/` input | claim extraction only | `research-only` |

## Mod scaffolding and examples

| Upstream source | Subject | Destination or owner | Decision | Status |
|---|---|---|---|---|
| `templates/mod-template/` | Mono BepInEx starter project | first-mod process and future project-owned starter | redesign, do not copy blindly | `extracted` |
| `mods/*/README.md` | User intent, features, install/use notes | examples, system pages, compatibility notes | extract | `planned-extract` |
| `mods/*/docs/research*` | Target discoveries and uncertainty | hook/system research records | extract and verify | `research-only` |
| `mods/*/docs/design*` and decisions | Design patterns and tradeoffs | process guidance and examples | extract | `planned-extract` |
| `mods/*/docs/compatibility*` | Historical profile and conflicts | versioned compatibility records | extract and reverify | `research-only` |
| `mods/*/docs/validation*`, review notes, test notes | Historical evidence | validation examples and research provenance | extract, never overclaim | `research-only` |
| `mods/*/release/` | Package layouts, versions, changelogs | release-process examples | extract | `planned-extract` |
| `mods/*/src/Plugin.cs`, patch and service code | Runtime patterns and target candidates | small project-owned examples plus hook records | extract facts/patterns | `extracted` at inventory level; semantic review pending |

## Existing selective ports

The current `Research/tainted-grail-system-ports/` track remains the canonical migration owner for:

- Tainted Framework;
- Tainted Interface;
- Avalon Core and Road Atlas;
- provider-neutral acquisition observations;
- Avalon AI API 2.0 authoring contracts;
- optional Merlin integration;
- separate inert Mono and IL2CPP route contracts.

The handbook links to those results and explains their use. It does not create a second framework, UI, road, AI, provider, or adapter contract.

## Blocked material

The following are not handbook migration inputs unless a separate review explicitly permits them:

- game managed assemblies or native binaries;
- BepInEx or Unity runtime binaries;
- compiled mod DLLs and release archives;
- wholesale decompiled game source;
- saves, credentials, private paths, unredacted logs, screenshots with private data, and live diagnostic dumps;
- upstream Tainted Interface visual payloads currently recorded with unresolved redistribution status;
- Git LFS payloads or third-party assets without an exact licence and redistribution decision;
- generated failed-incident bundles except redacted, approved evidence summaries.

## Completed first import

The inventory has driven the first handbook import:

- [`runtime/VERIFIED_PROFILES.md`](runtime/VERIFIED_PROFILES.md) records the exact pinned Mono and IL2CPP observations, their different evidence states, active-install verification requirements and prohibited inference;
- [`getting-started/FIRST_MOD_PATH.md`](getting-started/FIRST_MOD_PATH.md) records the complete profile, local-reference, identity, build, load, configuration, removal, evidence and package path;
- [`FILE_INVENTORY.md`](FILE_INVENTORY.md) gives every pinned source file a path-class disposition and names the first guide, template, patch, reflection, service, release and blocked-payload migration queues.

## Next inventory-driven unit

Create the first semantic hook batches from the named patch/reflection queues. Each selected source file must produce either:

- a complete candidate hook record;
- a system/reference fact linked to its canonical owner;
- a research blocker;
- or a rejection record explaining why it is unsafe, stale, duplicate, unlicensed or unsuitable for publication.