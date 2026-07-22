# Pinned Upstream File Inventory

## Inventory identity

This inventory is bound to:

- repository: `theb0yys/Tainted-Grail-The-Fall-of-Avalon-mods`;
- commit: `d7e740e7f167b73152b53409e483dab07d80d048`;
- upstream root licence: `NOASSERTION` at the pinned commit;
- inventory purpose: documentation and evidence migration only.

A path listed here is not automatically approved code, a supported hook, or runtime authority. The disposition controls what FOA-SDK may do with the source.

## Disposition vocabulary

| Disposition | Meaning |
|---|---|
| `adapt` | Rewrite the information for current FOA-SDK structure and verified profiles. |
| `extract` | Extract small facts, patterns, identifiers, targets, or examples with provenance. |
| `link` | FOA-SDK already has a canonical owner; the handbook links rather than duplicates. |
| `research` | Preserve as evidence input until claims are reverified. |
| `block` | Do not copy or publish the payload. |
| `supersede` | Retain historical provenance, but use the newer SDK contract or process. |

## Repository-wide path rules

These rules disposition every file at the pinned commit, including files not individually enumerated below.

| Path class | Content class | Disposition | Destination/owner | Required handling |
|---|---|---|---|---|
| `docs/*.md` | repository process and modding guidance | `adapt` or `supersede` | handbook process/reference pages | Preserve useful rules; replace stale runtime, validation, repository-layout, and release claims with current SDK contracts. |
| `docs/research/**` | dated research, taxonomies, system maps, compiled reports | `research` or `link` | `Research/`, catalog research, handbook research briefs | Extract claims with source/version/confidence; prose is never runtime authority. |
| `Research/**` | retained baseline corpus | `research` | `Research/` | Register, fingerprint, and cite; do not silently rewrite or promote. |
| `templates/mod-template/**` | starter project and examples | `extract` | first-mod guide and future project-owned example | Rebuild from current verified profiles; do not copy third-party or uncertain project metadata blindly. |
| `codex/**` | repository-specific AI agents and prompts | `extract` | contributor process only | Extract general review/checklist value; do not present repository automation as game/runtime truth. |
| `mods/*/README.md` | mod intent, install/use, feature description | `extract` | systems/examples/reference | Treat compatibility and support claims as historical until reverified. |
| `mods/*/docs/**` | research, design, decisions, compatibility, validation, review and test notes | `extract` or `research` | hooks, systems, process, examples, research briefs | Preserve uncertainty and exact source path. Runtime claims require profile evidence. |
| `mods/*/src/**` and `mods/*/tools/**` text source | plug-in entry points, patches, reflection, services, diagnostics, tooling | `extract` | hook and system candidate records | Extract only small patterns and target metadata. Do not bulk-port code. |
| `mods/*/release/*.md`, manifests and changelogs | release identity, package shape, version history | `extract` | packaging/release examples | Reconcile with current package manifests and legal review. |
| `mods/*/release/**` binaries and archives | compiled or packaged payloads | `block` | none | Never import into documentation or source. |
| `mods/*/codex_build/**`, `**/failed-incidents/**`, generated proof/output folders | generated evidence and local execution output | `block` or `research` | external evidence store | Only approved redacted summaries and fingerprints may be registered. |
| `**/*.dll`, `**/*.exe`, `**/*.pdb`, `**/*.zip`, `**/*.7z`, `**/*.rar` | binaries and archives | `block` | none | Never copy. |
| game/BepInEx/Unity assemblies or native payloads at any path | proprietary or third-party runtime material | `block` | local operator installation only | Reference by identity/fingerprint where permitted; never redistribute. |
| `**/*.png`, `**/*.jpg`, `**/*.psd`, `**/*.fbx`, `**/*.asset`, `**/*.prefab`, `**/*.unity`, `**/*.bundle` | visual, model, Unity, or asset payloads | `block` unless separately licensed | reviewed asset owner | Require exact ownership, licence, source, and redistribution decision. |
| `**/*.log`, saves, config exports, machine reports | private/local evidence | `block` by default | external redacted evidence | Remove private paths, account names, tokens, save data, and unrelated machine details. |

## Core guides and process files

| Exact path | Class | Disposition | Handbook result |
|---|---|---|---|
| `docs/INDEX.md` | ordered documentation index | `adapt` | handbook read order and role-based navigation |
| `docs/foa-modding-environment.md` | environment baseline | `adapt` | verified runtime profiles and local-reference rules |
| `docs/repo-layout.md` | source layout and prohibited files | `supersede` | current FOA-SDK ownership map plus retained prohibited-payload rules |
| `docs/engineering-process.md` | engineering workflow | `adapt` | research, design, implementation, review, validation, packaging and support process |
| `docs/code-review-standard.md` | review standard | `adapt` | findings-first mod code and target review |
| `docs/in-game-ui-quality-standard.md` | UI acceptance | `adapt` | UI system guidance and visual acceptance |
| `docs/compatibility-and-versioning.md` | compatibility policy | `adapt` | exact game/branch/runtime/loader/framework profiles |
| `docs/validation-matrix.md` | historical evidence levels | `supersede` | current typed evidence and exact-profile verification |
| `docs/gate-promotion-policy.md` | promotion boundary | `link` | governance engine and candidate-evidence rules |
| `docs/debugging-and-support.md` | support process | `adapt` | troubleshooting and report intake |
| `docs/diagnostic-tool-development-policy.md` | diagnostic safety | `adapt` | redaction, bounded capture and privacy rules |
| `docs/decision-records.md` | architecture decisions | `adapt` | mod decision-record template and lifecycle |
| `docs/mod-lifecycle.md` | complete mod lifecycle | `adapt` | handbook process spine |
| `docs/release-checklist.md` | packaging/release | `adapt` | current package, provenance and rollback checks |
| `docs/ecosystem/README.md` | identity governance | `adapt` | GUID, assembly, package and public-name ownership |
| `docs/ecosystem/renaming-process.md` | rename migration | `adapt` | identity-preserving rename process |

## Templates and first-mod source

| Exact path | Class | Disposition | Handbook result |
|---|---|---|---|
| `templates/mod-template/README.md` | scaffold instructions | `extract` | first-mod checklist |
| `templates/mod-template/src/FoAModTemplate.csproj` | project structure | `extract` | verified project-file explanation and generated example backlog |
| `templates/mod-template/src/Plugin.cs` | BepInEx entry point | `extract` | minimal load/log/config lifecycle example |
| `templates/mod-template/src/Patches/ExamplePatch.cs` | Harmony example | `extract` | candidate patch example; target must be replaced with a verified safe target |
| `templates/mod-template/docs/research.md` | mod research note | `adapt` | research template |
| `templates/mod-template/docs/design.md` | design note | `adapt` | design template |
| `templates/mod-template/docs/compatibility.md` | compatibility note | `adapt` | profile matrix template |
| `templates/mod-template/docs/validation-plan.md` | test plan | `adapt` | build/load/behavior/cleanup test template |
| `templates/mod-template/docs/review-notes.md` | review record | `adapt` | self-review and independent review template |
| `templates/mod-template/release/manifest.md` | package declaration | `adapt` | package inventory/provenance template |

## Patch-registration and Harmony candidate files

Every patch source remains a candidate until its exact target metadata and profile evidence are registered. The following files seed the first hook-extraction batches.

| Exact path | System area | Disposition | Initial state |
|---|---|---|---|
| `mods/wyrd-hunt/src/Patches/NpcDeathPatch.cs` | actor death/encounter | `extract` | candidate |
| `mods/wyrd-hunt/src/Patches/ExposureContextDiagnosticsPatch.cs` | survival/exposure diagnostics | `extract` | candidate |
| `mods/wyrd-hunt/src/Patches/ReadableStealPatch.cs` | interaction/crime | `extract` | candidate |
| `mods/wyrd-hunt/src/Patches/ContainerLootPatch.cs` | loot/economy | `extract` | candidate |
| `mods/crime-and-consequences/src/Patches/EconomyPatches.cs` | economy/crime | `extract` | candidate |
| `mods/crime-and-consequences/src/Patches/CrimeHunterSearchStatePatches.cs` | AI/crime state | `extract` | candidate |
| `mods/crime-and-consequences/src/Patches/StealthStatPatches.cs` | stealth/stats | `extract` | candidate |
| `mods/smart-save-backups/src/Patches/QuestCompletionPatch.cs` | quest/save trigger | `extract` | candidate; high save-risk review |
| `mods/smart-save-backups/src/Patches/StoryOfferChoicePatch.cs` | narrative/save trigger | `extract` | candidate; high save-risk review |
| `mods/smart-save-backups/src/Patches/CloudServiceEndSavePatch.cs` | persistence/cloud service | `extract` | candidate; high save-risk review |
| `mods/better-bonfire-menu/src/NativeBonfireServicesEntryPatch.cs` | UI/services | `extract` | candidate |
| `mods/better-bonfire-menu/src/NativeBonfireBackPatch.cs` | UI/navigation | `extract` | candidate |
| `mods/theB0yysSkillUncapper/src/Patches/ProficiencyCapPatch.cs` | progression | `extract` | candidate |
| `mods/origins-of-avalon/src/Patches/TitleScreenUtilsStartNewGamePatch.cs` | title screen/new game | `extract` | candidate; lifecycle-sensitive |
| `mods/immersive-progression/src/Patches/RestDiagnosticsPatch.cs` | rest/progression diagnostics | `extract` | candidate |
| `mods/immersive-progression/src/Patches/CraftingContextDiagnosticsPatch.cs` | crafting diagnostics | `extract` | candidate |
| `mods/immersive-progression/src/Patches/ProgressionMultiplierPatch.cs` | progression mutation | `extract` | candidate |
| `mods/immersive-progression/src/Patches/TradePracticePatch.cs` | trade/progression | `extract` | candidate |
| `mods/immersive-progression/src/Patches/ProficiencyXpDiagnosticsPatch.cs` | progression diagnostics | `extract` | candidate |
| `mods/immersive-progression/src/Patches/DamageContextDiagnosticsPatch.cs` | combat diagnostics | `extract` | candidate |
| `mods/immersive-progression/src/Patches/ProgressionSpendContextDiagnosticsPatch.cs` | progression diagnostics | `extract` | candidate |
| `mods/merchant-stock-tweaks/src/Patches/ShopOpenPatch.cs` | vendor inventory | `extract` | candidate |
| `mods/avalon-cheat-panel/src/Patches/HealthElementDamagePatch.cs` | combat/health | `extract` | candidate; example publication restricted |
| `mods/avalon-cheat-panel/src/Patches/HeroStatsCheatPatch.cs` | player stats | `extract` | candidate; example publication restricted |
| `mods/always-show-hud/src/Patches/HudElementVisibilityPatch.cs` | HUD/UI | `extract` | candidate |
| `mods/hold-to-steal/src/Patches/TheftSafetyPatches.cs` | interaction/crime | `extract` | candidate |
| `mods/food-drink-asset-proof/src/Patches/CustomBottleRecipeIntegrationPatch.cs` | items/recipes/assets | `extract` | candidate; asset payload blocked |
| `mods/tainted-survival/src/Patches/DiseaseCurseDiagnosticPatch.cs` | survival/status diagnostics | `extract` | candidate |

## Reflection and dynamic-target candidate files

| Exact path | Pattern | Disposition | Required verification |
|---|---|---|---|
| `mods/avalon-mounts/src/Plugin.cs` | dynamic target discovery | `extract` | exact type/member/signature and fallback behavior |
| `mods/avalon-spell-vfx/src/SpellCastPatch.cs` | reflective spell target | `extract` | target overload, lifecycle and allocation impact |
| `mods/multi-pin-map-notes/src/GameInputPatch.cs` | input target discovery | `extract` | input ownership, UI focus and cleanup |
| `mods/magic-tweaks/src/Patches/MagicCostPatch.cs` | reflective magic target | `extract` | target and value semantics |
| `mods/magic-tweaks/src/Patches/MagicDamagePatch.cs` | reflective damage target | `extract` | damage pipeline ordering and conflicts |
| `mods/magic-tweaks/src/Patches/StatusBuildupPatch.cs` | status-effect target | `extract` | target signature and status ownership |
| `mods/magic-tweaks/src/Patches/ProjectileLaunchPatch.cs` | projectile target | `extract` | object lifecycle and pooling |
| `mods/magic-tweaks/src/Patches/MagicAreaPatch.cs` | area-effect target | `extract` | allocation, ownership and cleanup |
| `mods/foa-mod-manager/src/Patches/MenuButtonPatch.cs` | runtime UI lookup | `extract` | Mono/IL2CPP-specific lookup separation |
| `mods/TaintedEconomy/src/Patches/PatchRegistration.cs` | centralized target registration | `extract` | every registered target and failure mode |
| `mods/Tainted Combat/src/Patches/StaggerRewritePatch.cs` | dynamic combat target | `extract` | exact target, prefix/transpiler risk and conflicts |
| `mods/Tainted Combat/src/Patches/MovementStaminaPatch.cs` | movement/stamina target | `extract` | per-frame cost and compatibility |
| `mods/avalon-companions/src/Patches/PanelInputLockPatch.cs` | UI/input target | `extract` | panel lifecycle and unpatch behavior |
| `mods/origins-of-avalon/src/Patches/OriginRuntimeHookBridge.cs` | runtime hook bridge | `extract` | event/patch ownership and cleanup |
| `mods/always-show-hud/src/Patches/HudElementVisibilityPatch.cs` | reflected UI elements | `extract` | null/stale object handling |
| `mods/Avalon Stash/src/Patches/StorageSummaryLabelPatch.cs` | inventory UI target | `extract` | UI instance lifecycle and localization |
| `mods/Avalon Stash/src/Patches/IngredientCountTextPatches.cs` | crafting UI targets | `extract` | overloads, localization and conflicts |

## Service, manager, event and lifecycle candidates

Files matching these patterns are inventoried as `extract` and require explicit subscription/cleanup records:

- plug-in lifecycle entry points: `mods/*/src/Plugin.cs`;
- manager and registry access: files containing `Manager`, `Registry`, `Service`, `Singleton`, `Instance`, or `Context` in source names or target references;
- event subscriptions: source containing `+=`, `-=`, Unity lifecycle methods, scene-load callbacks, input callbacks, or framework event buses;
- centralized patch installers: `PatchRegistration.cs`, `PatchPlan.cs`, `HookBridge.cs`, and equivalent registrars;
- diagnostics probes: `*Probe.cs`, `*Diagnostics*.cs`, `*Detector.cs`, and bounded capture tools.

Known seed files include:

- `mods/avalon-core/src/Core/AvalonEngineContext.cs` — canonical framework/context ownership, now linked to the existing SDK system-port owner;
- `mods/living-avalon/src/Patches/WorldPopulationPatchPlan.cs` — world/spawn patch planning candidate;
- `mods/avalon-mounts/src/Diagnostics/MountModDetector.cs` — compatibility detection candidate;
- `mods/avalon-awakened/src/CreatorInventoryProbe.cs` — creator/runtime inventory research;
- `mods/avalon-awakened/src/CompleteCharacterRuntimeInventory.cs` — character-runtime inventory research;
- `mods/magic-tweaks/src/WeightboundDiagnostics.cs` — diagnostics candidate;
- `mods/avalon-exceptions/src/BepInExDependencyErrorProbe.cs` — loader/dependency diagnostics candidate.

## Release records

All Markdown release manifests and changelogs under `mods/*/release/` are `extract`. They may supply:

- stable mod identity and version history;
- intended package layout;
- dependency declarations;
- compatibility wording;
- validation claims requiring reverification;
- rollback and support expectations.

Known seed manifests include:

- `mods/easy-avalon/release/manifest.md`;
- `mods/Avalon Stash/release/manifest.md`;
- `mods/magic-tweaks/release/manifest.md`;
- `mods/hello-avalon/release/manifest.md`;
- `mods/hold-to-steal/release/manifest.md`;
- `mods/avalon-mounts/release/manifest.md`;
- `mods/tainted-blood/release/manifest.md`.

Compiled DLLs, archives, signing material, upload receipts and generated package directories remain `block`.

## Existing canonical-owner links

The following upstream families are not reimplemented by this handbook:

| Upstream family | Canonical FOA-SDK owner | Disposition |
|---|---|---|
| `mods/tainted-framework/**` | Tainted Framework knowledge, ExtensionAPI and editor services | `link`; extract only public explanatory examples |
| `mods/Tainted Interface/**` | Tainted Interface semantic catalog and approved project-owned fallbacks | `link`; upstream visual payloads remain blocked |
| `mods/avalon-core/**` | catalog/game knowledge, Road Atlas and system-port research | `link`; extract hook/system facts only |
| `mods/avalon-ai-runtime/**` | Avalon AI API 2.0 authoring contracts | `link`; runtime execution remains unavailable |
| `mods/foa-mod-manager/**` | integration/runtime research | `research`; no current combined runtime authority |
| `mods/template-diagnostics/**` | diagnostics/intake research | `link/research`; capture remains separately governed |
| `mods/avalon-awakened/**` | Kandra/Unity conversion research | `research`; creator assets and runtime payloads blocked |

## Completion rule

This inventory is complete at the path-policy level: every file at the pinned commit falls under one repository-wide rule. Named rows are the first migration queue, not an assertion that every candidate has been semantically reviewed.

A migration batch is complete only when each selected file has:

1. exact path and pinned commit;
2. content class and disposition;
3. destination owner;
4. licence/redistribution state;
5. game, branch, runtime and loader scope where applicable;
6. extracted claims or hook targets;
7. research and validation blockers;
8. resulting handbook, research-register, catalog or rejected-payload reference.
