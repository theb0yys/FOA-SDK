# Tainted Grail Modding Infrastructure

This folder records the shared infrastructure boundaries used by the Tainted Grail Modding SDK and Editor.

## Active build modules

The first two scaffolded boundaries now have real internal build targets while source paths remain stable under `Code/Source`:

- `TaintedCore/` maps to `TaintedGrailModdingSDK.Core.Static` for shared domain primitives and pure core services;
- `TaintedFramework/` maps to `TaintedGrailModdingSDK.Framework.Static` for persistence, path policy, workspace loading, Foundation orchestration, and native source-asset processing services;
- the Tool Gem `Editor` target owns Qt widgets, the Editor system component, and module composition.

`Core.Static` and `Framework.Static` are internal implementation targets. Only the existing Editor module is exposed through the Tool and Builder aliases.

## Reserved scaffold modules

These folders remain scaffolding only and have no build target or runtime behavior:

- `TaintedUI/` - future reusable Editor UI components and presentation contracts;
- `Exceptions/` - future error types, diagnostics, and failure-reporting contracts;
- `AI/` - future AI-facing contracts and integrations approved by project research.

Implementations remain evidence-gated: each new module needs a documented contract, ownership boundary, dependencies, and validation requirements before build integration.

## Dependency direction

`Core.Static` is the lowest shared layer. `Framework.Static` depends on Core, and Editor depends on Framework. Core must not depend on Framework, Editor, UI, AI, Qt, AzToolsFramework, or runtime integrations. Cross-module behavior is exposed through existing documented C++ interfaces rather than duplicate compilation or reverse implementation coupling.

Tests link `Framework.Static` and receive Core transitively. Test manifests own tests only; every production translation unit has exactly one production target owner.

## Runtime boundary

This build split remains entirely editor-side. It adds no FoA runtime adapter, game launch, deployment, injection, save mutation, or telemetry behavior.

The Framework also owns `SourceShaderBuilderComponent` and the private
`SourceShaderRenderComponent` automation boundary. Windows builds link pinned
Atom DX12 reflection and RPI Edit libraries to create native assets from exact
source bytecode. The engine checkout remains external and unchanged. Source
program packets, textures, native products and pixel evidence stay outside source
control. See `docs/tainted-grail-sdk/CAMPAIGN_SCENE_EDITING.md` for packet versions,
resource/lifetime bounds and the limited native shader acceptance scope.

`SourceScenePlacementComponent` also belongs to Framework and is registered by
the Editor module. It is an editor entity component, not a required system or game
runtime component. Its private source binding and full-affine placement contract
are documented in `docs/tainted-grail-sdk/CAMPAIGN_SCENE_EDITING.md`.

SourceSceneRenderComponent is a Framework-owned editor component. It binds a private source draw to SourceScenePlacementComponent, routes resource ownership through SourceShaderRenderComponent, and follows native transform, visibility and activation events. It does not select game passes or provide runtime export authority.
