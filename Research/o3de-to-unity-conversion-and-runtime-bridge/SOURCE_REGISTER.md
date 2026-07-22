# Source Register

Baseline commit for pre-change repository observations:
`92aa29960bab92d646c464ae48b8cf09d881a436` (`FOA-plug-in-development`). Gate 0 entries marked "this commit"
refer to the exact Git commit containing this register, so the containing commit ID is their durable revision.

External retrieval date unless otherwise stated: 20 July 2026.

Registering a source does not approve a component, version, licence, experiment, or operation. Exact versions
must be rechecked at the gate that would consume them.

## Preserved input

| ID | Source | Version or identity | Supports | Limitation |
| --- | --- | --- | --- | --- |
| `SRC-INPUT-001` | `inputs/FOA_SDK_O3DE_TO_UNITY_CONVERSION_AND_RUNTIME_BRIDGE_RESEARCH_REPORT.md` | SHA-256 `b17850a12efe97dbd92a8bdf9cfcd155204105c49e230c51cf9b10aceba9c048` | Supplied research hypotheses and its internal evidence ledger | Its `turn...` citation tokens are opaque and non-durable; the report is not an accepted decision |
| `SRC-INPUT-002` | `inputs/CANONICAL_INTERCHANGE_SCHEMA_AND_IDENTITY_DEEP_RESEARCH.md` | containing Git revision; normalized intake observed 22 July 2026 | Deep Research schema, identity, payload, loss, legal, fixture, and register proposals | Normalized substantive preservation rather than raw rendered-artifact bytes; stale baseline and proposals require reconciliation |

## Repository sources

Unless explicitly marked "this commit", paths in this section were observed at the baseline commit above.

| ID | Repository path | Supports | Limitation |
| --- | --- | --- | --- |
| `SRC-REPO-001` | `README.md` | Pre-alpha status; governed O3DE editor; no safe runtime-executor claim | Project status can change after this commit |
| `SRC-REPO-002` | `docs/tainted-grail-sdk/ARCHITECTURE.md` | O3DE host/Unity runtime separation; Core, Framework, Editor, diagnostic, and runtime-adapter ownership | Does not approve a concrete runtime adapter |
| `SRC-REPO-003` | `docs/tainted-grail-sdk/EDITOR_TOOLCHAIN_UNITY_INTERCHANGE_DESIGN.md` | This commit: approved inert Core-only Gate 0; Blender-first provider program; file interchange; identities; losses; and Gates 1–13 | Gate 0 is in development; broader Phase 9 provider/execution program remains proposed |
| `SRC-REPO-004` | `ROADMAP.md` | This commit: Gate 0 contract-only exception, Phase 9 entry for Gates 1+, ecosystem ordering, and Unity Editor/runtime separation | Gate 0 adds no service, process, filesystem, provider, build, deployment, or runtime authority |
| `SRC-REPO-005` | `docs/tainted-grail-sdk/SDK_FUNCTIONAL_EXPANSION_PROGRAM.md` | First-party capability items 1–7 before extension/automation; shared layer rules | Does not approve a provider version |
| `SRC-REPO-006` | `Gems/ExternalToolchain/gem.json` | `ExternalToolchain` Gem version `0.2.0` | Gem version is distinct from host API version |
| `SRC-REPO-007` | `Gems/ExternalToolchain/Code/Include/ExternalToolchain/ExternalToolchainTypes.h` | Host API `1.1.0`; `EngineBridge` family; typed provider descriptors | Current API has no execution service |
| `SRC-REPO-008` | `Gems/ExternalToolchain/docs/ARCHITECTURE.md` | Generic host ownership, discovery-only current state, and ordered follow-on work | Follow-on list is not implementation authority |
| `SRC-REPO-009` | `Gems/ExternalToolchain/docs/DISCOVERY_AND_CONFIGURATION.md` | Configured-path-only discovery; no interrogation, recursive scan, registry scan, package-manager scan, or execution | Configured versions are caller inputs, not executable observations |
| `SRC-REPO-010` | `Code/Framework/AzFramework/AzFramework/Process/ProcessWatcher.h` and platform implementations | Existing O3DE process-launch primitive, vector/string command parameter representation, tether flag | A primitive does not prove secure supervision, cancellation, process-tree cleanup, or sandboxing |
| `SRC-REPO-011` | `docs/tainted-grail-sdk/FOA_ADAPTER_WORK_ORDER_PLANS.md` | Canonical runtime work-order plans retain `ExecutionAllowed: false` | Plans are not executable handoffs |
| `SRC-REPO-012` | `docs/tainted-grail-sdk/FOA_ADAPTER_BUILD_MANIFESTS.md` | Build manifests retain `BuildAllowed: false` | Manifests do not compile or package output |
| `SRC-REPO-013` | `docs/tainted-grail-sdk/FOA_ADAPTER_RUNTIME_RESULT_EVIDENCE.md` | Typed runtime-result candidate-evidence concepts | Runtime evidence is not a generic process-result contract |
| `SRC-REPO-014` | `docs/tainted-grail-sdk/RELEASE_PROCESS.md` | Package, confirmation, deployment, verification, and release authority remain separately gated | Current contracts do not invoke an executor |
| `SRC-REPO-015` | `docs/tainted-grail-sdk/LEGAL_AND_CONTENT_POLICY.md` | Allowed original/synthetic material and prohibited proprietary/private contributions | Project policy is not legal advice or a blanket licence determination |
| `SRC-REPO-016` | `Gems/AtomLyIntegration/TechnicalArt/DccScriptingInterface/` | In-tree DCC bootstrap and Blender prototype material exists | Presence does not establish current Blender compatibility or qualification |
| `SRC-REPO-017` | `Gems/SceneProcessing/` | Scene-processing Gem exists in the fork | Existence does not prove project activation or the planned interchange handoff |
| `SRC-REPO-018` | `docs/tainted-grail-sdk/EXTERNAL_TOOL_INTERCHANGE_GATE_0.md` | This commit: exact Gate 0 contracts, disabled authority matrix, pure validation, and acceptance boundary | Gate 0 is in development and grants no permission to begin Gate 1 |

## Direct official external sources

| ID | Publisher | Direct URL | Version/date scope | Supports | Limitation |
| --- | --- | --- | --- | --- | --- |
| `SRC-O3DE-001` | Open 3D Engine | https://docs.o3de.org/docs/user-guide/assets/scene-settings/scene-format-support/ | Retrieved 2026-07-20 | Published scene-format support description | Must be reconciled to the exact fork and qualified importer behavior |
| `SRC-O3DE-002` | Open 3D Engine | https://docs.o3de.org/docs/user-guide/assets/scene-settings/ | Retrieved 2026-07-20 | Scene source and Asset Processor concepts | Does not prove transactional publication for this SDK |
| `SRC-O3DE-003` | Open 3D Engine | https://www.docs.o3de.org/docs/release-notes/archive/2305-0-release-notes/ | O3DE 23.05 release notes; retrieved 2026-07-20 | Historical DCCsi/Blender prototype context | Historical testing cannot qualify a current Blender release |
| `SRC-O3DE-004` | Open 3D Engine | https://docs.o3de.org/docs/api/frameworks/azframework/struct_az_framework_1_1_process_launcher_1_1_process_launch_info | Retrieved 2026-07-20 | Official `ProcessLaunchInfo` API fields | A launch primitive does not establish safe supervision |
| `SRC-O3DE-005` | Open 3D Engine | https://docs.o3de.org/docs/user-guide/gems/reference/debug/remote-tools/ | Retrieved 2026-07-20 | Remote Tools is a debugging-oriented networking facility | It is not selected as an interchange authority channel |
| `SRC-BLENDER-001` | Blender Foundation | https://www.blender.org/download/ | Retrieved 2026-07-20 | Current download/release observation | Current release status is version-sensitive and not a support declaration |
| `SRC-BLENDER-002` | Blender Foundation | https://docs.blender.org/manual/en/latest/files/import_export/fbx.html | `latest` manual retrieved 2026-07-20 | Blender FBX importer/exporter behavior | `latest` is mutable; pin the accepted Blender/manual version at qualification |
| `SRC-UNITY-001` | Unity Technologies | https://docs.unity3d.com/6000.5/Documentation/Manual/EditorCommandLineArguments.html | Unity 6.5 manual, retrieved 2026-07-20 | Documented Editor command-line and batch-operation surface | Exact flags and behavior must be checked against the selected Editor version rather than inferred from 6.5 |
| `SRC-UNITY-002` | Unity Technologies | https://docs.unity3d.com/6000.4/Documentation/Manual/AssetMetadata.html | Unity 6.4 manual, retrieved 2026-07-20 | Unity asset metadata and GUID concepts | Does not define the FOA-SDK canonical-to-Unity binding strategy |
| `SRC-UNITY-003` | Unity Technologies | https://docs.unity3d.com/ScriptReference/AssetImporters.ScriptedImporter.html | Current scripting API, retrieved 2026-07-20 | Unity custom importer extension point | Availability and behavior remain selected-version concerns |
| `SRC-UNITY-004` | Unity Technologies | https://docs.unity3d.com/ScriptReference/AssetPostprocessor.html | Retrieved 2026-07-20 | Unity import post-processing extension point | Import code execution remains a high-risk operation |
| `SRC-UNITY-005` | Unity Technologies | https://docs.unity3d.com/Packages/com.unity.formats.fbx@5.1/manual/index.html | FBX Exporter 5.1 documentation; retrieved 2026-07-20 | Unity FBX package candidate surface | Does not approve version `5.1.6` or compatibility with the selected Unity profile |
| `SRC-UNITY-006` | Unity Technologies | https://docs.unity3d.com/Packages/com.unity.formats.fbx@5.1/license/LICENSE.html | FBX Exporter 5.1 licence page; retrieved 2026-07-20 | Candidate licence text | Requires exact-package and distribution review |
| `SRC-UNITY-007` | Unity Technologies | https://docs.unity3d.com/Packages/com.unity.cloud.gltfast@6.15/manual/installation.html | glTFast 6.15 documentation; retrieved 2026-07-20 | Later glTF candidate | Does not establish O3DE round-trip support |
| `SRC-UNITY-008` | Unity Technologies | https://docs.unity.cn/Packages/com.unity.formats.usd%403.0/manual/index.html | USD 3.0.0-exp.4 documentation dated 2023-06-23; retrieved 2026-07-20 | Historical experimental USD package research | Experimental historical documentation cannot select a current Unity package |
| `SRC-UNITY-009` | Unity Technologies | https://docs.unity3d.com/6000.5/Documentation/Manual/AssetBundlesIntro.html | Unity 6.5 manual, retrieved 2026-07-20 | AssetBundle assembly and compatibility constraints | Does not establish that FoA uses or can load SDK-generated bundles |
| `SRC-UNITY-010` | Unity Technologies | https://docs.unity3d.com/6000.0/Documentation/Manual/assembly-definitions-creating.html | Unity 6.0 manual, retrieved 2026-07-20 | Editor-only assembly-definition boundary | Exact package/project behavior remains a Gate 8 fixture concern |
| `SRC-BEPINEX-001` | BepInEx project | https://docs.bepinex.dev/master/articles/user_guide/installation/index.html | `master` documentation retrieved 2026-07-20 | Separate installation paths and runtime-target research | Mutable documentation; no FoA target is selected or qualified |
| `SRC-BEPINEX-002` | BepInEx project | https://docs.bepinex.dev/master/articles/dev_guide/plugin_tutorial/2_plugin_start.html | `master` documentation retrieved 2026-07-20 | Distinct Mono and IL2CPP plugin base classes and support assemblies | Does not approve BepInEx compilation or execution in this project |
| `SRC-BEPINEX-003` | BepInEx project | https://docs.bepinex.dev/master/api/index.html | `master` API documentation retrieved 2026-07-20 | Published BepInEx API surface | Mutable documentation; no FoA compatibility follows |
| `SRC-MS-001` | Microsoft | https://learn.microsoft.com/en-us/dotnet/csharp/language-reference/compiler-options/code-generation | Page updated 2025-08-07; retrieved 2026-07-20 | C# deterministic compiler option | Compiler determinism does not prove whole-package or Unity-output determinism |

## Sources not yet durable

The supplied report still refers to official store/news material, Unity licensing terms, Addressables documentation,
BepInEx release channels, and community reports about FoA Mono/IL2CPP branches only through opaque `turn...`
citations. Until the exact direct URL, relevant version or revision, retrieval date, and scoped claim are added
here, those claims remain `unverified` and cannot authorize design or implementation.

## Current reconciliation sources

Observed 22 July 2026 against `main` commit `8fb3f0a729e4be4e513ba896ba52708a73d03eae`.

| ID | Durable source | Supports | Limitation |
| --- | --- | --- | --- |
| `SRC-REPO-019` | https://github.com/theb0yys/FOA-SDK/blob/8fb3f0a729e4be4e513ba896ba52708a73d03eae/README.md#L5-L56 | Current product identity, O3DE authoring/FoA runtime separation, product layout, and non-authority status | Product state may change after this commit |
| `SRC-REPO-020` | https://github.com/theb0yys/FOA-SDK/compare/195f5c15f0c59d47bd54e661b37d7457af9d1d95...8fb3f0a729e4be4e513ba896ba52708a73d03eae | Exact baseline reconciliation; eight commits limited to Suite Wizard catalogue discovery, installer documentation, and tests | Does not prove later branch state |
| `SRC-REPO-021` | https://github.com/theb0yys/FOA-SDK/blob/8fb3f0a729e4be4e513ba896ba52708a73d03eae/Plugins/README.md#L37-L85 | Authoring, integration, and runtime-adapter category ownership; Mono/IL2CPP package separation | Package declaration does not grant operational authority |
| `SRC-REPO-022` | https://github.com/theb0yys/FOA-SDK/blob/8fb3f0a729e4be4e513ba896ba52708a73d03eae/Gems/TaintedGrailModdingSDK/Code/Source/FoARuntimeAdapterRoutes.cpp#L99-L183 | Exact Mono and IL2CPP route observations, provenance, evidence states, and blocked capabilities | Route observations do not select an active operator target or authorize execution |
| `SRC-REPO-023` | https://github.com/theb0yys/FOA-SDK/blob/2e45192157cfe4fa5086330f99bb0cb82ed18111/Research/o3de-to-unity-conversion-and-runtime-bridge/areas/06-runtime-target-profile/README.md | Correct separation of pinned route evidence from active operator-profile selection | This commit is research-only and grants no diagnostic authority |
| `SRC-REPO-024` | https://github.com/theb0yys/FOA-SDK/blob/2e45192157cfe4fa5086330f99bb0cb82ed18111/Research/o3de-to-unity-conversion-and-runtime-bridge/areas/07-foa-runtime-adapters/README.md | Runtime-adapter separation, re-entry order, and current blocked capabilities | No executable adapter package is approved |
| `SRC-REPO-025` | https://github.com/theb0yys/FOA-SDK/blob/8fb3f0a729e4be4e513ba896ba52708a73d03eae/Research/o3de-to-unity-conversion-and-runtime-bridge/gates/IMPLEMENTATION_GATE_MAPPING.md#L13-L88 | Controlling gate order and rejected reorderings | Mapping does not satisfy any gate |

## Canonical interchange reconciliation sources

Observed 22 July 2026 against current `main` commit `504e10b27e46fceae4d68af200118edca27b4d1b`.

| ID | Durable source | Supports | Limitation |
| --- | --- | --- | --- |
| `SRC-REPO-026` | https://github.com/theb0yys/FOA-SDK/compare/195f5c15f0c59d47bd54e661b37d7457af9d1d95...504e10b27e46fceae4d68af200118edca27b4d1b | Exact 179-commit drift reconciliation | Broad comparison; individual claims still require scoped files |
| `SRC-REPO-027` | https://github.com/theb0yys/FOA-SDK/blob/504e10b27e46fceae4d68af200118edca27b4d1b/README.md | Current product identity and authoring/runtime separation | Product state may change after this commit |
| `SRC-REPO-028` | https://github.com/theb0yys/FOA-SDK/blob/504e10b27e46fceae4d68af200118edca27b4d1b/docs/tainted-grail-sdk/ARCHITECTURE.md | Exact identity, ownership, evidence, and fail-closed architecture | Does not itself select Schema-1 field shapes |
| `SRC-REPO-029` | https://github.com/theb0yys/FOA-SDK/blob/504e10b27e46fceae4d68af200118edca27b4d1b/docs/tainted-grail-sdk/EDITOR_TOOLCHAIN_UNITY_INTERCHANGE_DESIGN.md | Manifest-and-payload design, identity, loss, lock, security, test, and gate requirements | Gates 1–13 remain separately reviewed |
| `SRC-REPO-030` | https://github.com/theb0yys/FOA-SDK/blob/504e10b27e46fceae4d68af200118edca27b4d1b/Research/o3de-to-unity-conversion-and-runtime-bridge/gates/IMPLEMENTATION_GATE_MAPPING.md | Gate 5 placement and continued Phase 9 prerequisite | Does not authorize Gate 5 |
| `SRC-REPO-031` | https://github.com/theb0yys/FOA-SDK/blob/504e10b27e46fceae4d68af200118edca27b4d1b/Gems/TaintedGrailModdingSDK/Code/Source/DeterministicContractJson.h | Current byte-oriented deterministic JSON and numeric serialization behavior | Generic helper is not automatically the Schema-1 canonical profile |
| `SRC-REPO-032` | https://github.com/theb0yys/FOA-SDK/blob/504e10b27e46fceae4d68af200118edca27b4d1b/Gems/TaintedGrailModdingSDK/Code/Tests/DeterministicContractJsonTests.cpp | Exact current non-finite, round-trip precision, and escaped-byte behavior | Focused helper tests do not prove interchange canonicalization |
| `SRC-REPO-033` | https://github.com/theb0yys/FOA-SDK/blob/504e10b27e46fceae4d68af200118edca27b4d1b/Gems/TaintedGrailModdingSDK/Code/Source/FoundationModels.h | Existing workspace, profile, pack, source, evidence, catalog, identity, validation, and governance fields | Existing models must not be copied into a parallel authority system |
| `SRC-REPO-034` | https://github.com/theb0yys/FOA-SDK/blob/504e10b27e46fceae4d68af200118edca27b4d1b/o3de.lock.json | Exact O3DE 2.7.0 lock at `68683f23fb747380d3efa2424bd5f30242e9c5a2` | Engine pin does not qualify a provider or payload format |
| `SRC-O3DE-006` | https://www.docs.o3de.org/docs/user-guide/assets/scene-settings/scene-format-support/ | O3DE right-handed Z-up basis, forward orientation, metre unit, and conversion need | Documentation does not prove cross-host fixture success |
| `SRC-O3DE-007` | https://docs.o3de.org/docs/api/frameworks/azcore/_a_z_base_math.html | O3DE standard matrix-on-left, column-vector transform form | Does not select serialized storage by itself |
| `SRC-BLENDER-003` | https://docs.blender.org/manual/en/4.2/files/import_export/stl.html | Blender Y-forward, Z-up export/import axis declarations | STL page documents Blender basis but does not qualify a scene payload format |
| `SRC-UNITY-011` | https://docs.unity3d.com/current/Manual/QuaternionAndEulerRotationsInUnity.html | Unity left-handed, X-right, Y-up, Z-forward basis | Mutable current documentation; exact accepted Editor remains later qualification |
| `SRC-RFC-001` | https://www.rfc-editor.org/rfc/rfc8785.html | Comparative JSON canonicalization, duplicate-key and deterministic serialization considerations | Accepted Schema-1 profile is not an unqualified JCS implementation |
| `SRC-UNICODE-001` | https://www.unicode.org/reports/tr15/tr15-57.html | Unicode normalization definitions and conformance implications | Schema 1 deliberately does not normalize Unicode |

The accepted Area 02 research conclusion is
`areas/02-authoring-interchange-and-assets/CANONICAL_INTERCHANGE_SCHEMA_AND_IDENTITY_RESEARCH.md`. It remains
research-only and is not implementation authority.
