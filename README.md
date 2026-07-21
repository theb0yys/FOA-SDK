# FOA-SDK

FOA-SDK is an unofficial, open-source world-authoring and mod-development platform for **Tainted Grail: The Fall of Avalon**.

The project uses O3DE as a governed authoring host while keeping the target game and its Unity runtime outside the editor repository. Editor tooling, content schemas, validation, packaging, AI-assisted authoring, external conversion, and runtime adapters are separated into reviewable systems.

> **Status:** pre-alpha development. This repository is not a finished SDK, does not provide a supported public release, and does not grant runtime compatibility or deployment authority.

## Repository identity

FOA-SDK is the product repository. It is **not** an O3DE source fork.

The supported checkout layout is:

```text
Development/
├── FOA-SDK/      product source
├── o3de/         exact upstream engine checkout
└── foa-build/    generated build and evidence output
```

The exact O3DE repository, engine identity, version, and commit are recorded in [`o3de.lock.json`](o3de.lock.json). Tooling fails closed when the selected checkout does not match that lock.

Generated builds, caches, diagnostics, screenshots, installers, packages, and validation receipts belong under `foa-build/` or another reviewed output directory—not in either source checkout.

## Product structure

```text
.github/                              FOA issue forms, ownership and manual workflows
Gems/
├── ExternalToolchain/                bounded external-tool discovery and provider contracts
└── TaintedGrailModdingSDK/           editor, schemas, services, validators and tests
Research/                             preserved research inputs and reviewed conclusions
TaintedGrailModdingEditor/            dedicated O3DE Editor project

docs/tainted-grail-sdk/               public architecture, user and contributor documentation

o3de.lock.json                        exact external engine dependency
README.md                              product overview
ROADMAP.md                             planned capability sequence
CHANGELOG.md                           reviewed capability history
GOVERNANCE.md                         project governance
CONTRIBUTING.md                        contribution requirements
SECURITY.md                            vulnerability reporting policy
SUPPORT.md                             support boundaries
LICENSE.txt                            repository licence
```

Stock O3DE source, engine Gems, templates, scripts, assets, registries, and build files are deliberately absent. They are supplied by the separately pinned engine checkout.

## Obtain the pinned engine

From the directory that contains FOA-SDK:

```powershell
git clone https://github.com/o3de/o3de.git o3de
git -C o3de checkout 68683f23fb747380d3efa2424bd5f30242e9c5a2
```

Do not substitute a branch tip. Engine changes require a reviewed update to `o3de.lock.json` with configure, build, compiled-test, and Editor acceptance evidence.

Set `FOA_O3DE_ROOT` only when the engine is not the sibling `o3de/` checkout. Set `FOA_BUILD_ROOT` only when generated output should not use the sibling `foa-build/` directory.

## Developer Preview command path

Run commands from the FOA-SDK product checkout.

### Check prerequisites

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview.py prerequisites `
  --engine-root ..\o3de `
  --build-dir ..\foa-build\tg-sdk-developer-preview-0-windows-profile
```

### Configure

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview.py configure `
  --engine-root ..\o3de `
  --build-dir ..\foa-build\tg-sdk-developer-preview-0-windows-profile
```

The generated command uses the external engine as the CMake source and the product-owned project as `LY_PROJECTS`:

```text
-S <engine-root>
-B <build-root>
-DLY_PROJECTS=<product-root>/TaintedGrailModdingEditor
```

### Build

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview.py build `
  --engine-root ..\o3de `
  --build-dir ..\foa-build\tg-sdk-developer-preview-0-windows-profile
```

The current command-layer targets are:

```text
Editor
AssetProcessorBatch
TaintedGrailModdingSDK.Catalog.Tests
```

### Validate

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview.py validate `
  --engine-root ..\o3de `
  --build-dir ..\foa-build\tg-sdk-developer-preview-0-windows-profile
```

For the complete local gate, including both custom Gems and O3DE source-policy validation:

```powershell
python Gems/TaintedGrailModdingSDK/Tools/run_local_validation.py `
  --engine-root ..\o3de `
  --ctest-build-dir ..\foa-build\tg-sdk-developer-preview-0-windows-profile
```

The local gate does not claim a hosted CI result. Repository Actions remain manual-only until the reviewed runner policy is restored.

## Architecture boundary

FOA-SDK owns authoring data, Editor panes, typed contracts, validation, evidence, packaging previews, and governed work-order planning.

O3DE owns engine and host functionality. The target Unity game owns final runtime interpretation. Cross-engine conversion is performed through deterministic, reviewable file handoff; Unity-native files remain Unity-owned.

No editor action silently promotes evidence, mutates a game installation, launches FoA, injects code, modifies saves, signs artifacts, or publishes a release. Mono and IL2CPP runtime support remain separate adapter paths and require exact-install evidence.

## Documentation

Start with:

- [FOA-SDK documentation index](docs/tainted-grail-sdk/README.md)
- [Architecture](docs/tainted-grail-sdk/ARCHITECTURE.md)
- [Development guide](docs/tainted-grail-sdk/DEVELOPMENT_GUIDE.md)
- [Developer Preview 0](docs/tainted-grail-sdk/DEVELOPER_PREVIEW_0.md)
- [External O3DE dependency boundary](docs/tainted-grail-sdk/EXTERNAL_O3DE_DEPENDENCY.md)
- [Open and test the Editor](docs/tainted-grail-sdk/OPEN_AND_TEST_EDITOR.md)
- [CI and local validation](docs/tainted-grail-sdk/CI_AND_LOCAL_VALIDATION.md)
- [Review and merge policy](docs/tainted-grail-sdk/REVIEW_AND_MERGE_POLICY.md)
- [Legal and content policy](docs/tainted-grail-sdk/LEGAL_AND_CONTENT_POLICY.md)

## Branch and review model

- `main` is reviewed integrated product state.
- Development occurs on short-lived branches and enters `main` through pull requests.
- Significant changes require design review, focused validation, exact-head evidence where applicable, and maintainer approval.
- Direct runtime authority, automatic evidence promotion, and unreviewed generated artifacts are prohibited.

See [CONTRIBUTING.md](CONTRIBUTING.md), [GOVERNANCE.md](GOVERNANCE.md), and the [review and merge policy](docs/tainted-grail-sdk/REVIEW_AND_MERGE_POLICY.md).

## Legal status

Tainted Grail: The Fall of Avalon and related names, assets, and marks belong to their respective rights holders. FOA-SDK is an independent community project and is not affiliated with or endorsed by the game’s developers or publishers.

Do not contribute proprietary game source, copyrighted assets without permission, credentials, private paths, saves, extracted commercial content, or material whose redistribution status is unknown.
