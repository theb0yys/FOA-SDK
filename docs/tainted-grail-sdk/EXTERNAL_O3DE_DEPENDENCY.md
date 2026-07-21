# External O3DE dependency boundary

Status: FOA-only working tree implemented; filtered-history migration pending.

FOA-SDK is the product. O3DE is the authoring engine dependency. The supported layout separates three roots:

```text
Development/
├── FOA-SDK/      product_root
├── o3de/         engine_root
└── foa-build/    build_root
```

The roots must not contain one another. Generated build, validation, diagnostics, screenshot, installer, and release outputs do not belong in either source checkout.

## Product working tree

The FOA-SDK working tree contains only product-owned material:

```text
.github/
Gems/ExternalToolchain/
Gems/TaintedGrailModdingSDK/
Research/
TaintedGrailModdingEditor/
docs/tainted-grail-sdk/
product governance and dependency files
```

Stock O3DE source roots, engine Gems, templates, assets, registries, scripts, CMake entry points, `engine.json`, and `CMakePresets.json` are forbidden by the extracted-product validator.

Current FOA paths are intentionally preserved during this first extraction stage. Directory renaming or deeper reorganisation is a later, independent change.

## Exact engine lock

`o3de.lock.json` is the machine-readable engine contract. The initial reviewed pin is:

```text
repository: https://github.com/o3de/o3de.git
commit: 68683f23fb747380d3efa2424bd5f30242e9c5a2
engine: o3de
version: 2.7.0
```

The commit is the upstream O3DE commit titled `O3DE Fetch Content (#19622)`. Developer Preview commands fail closed when the checkout does not resolve to that exact commit or when `engine.json` reports a different engine identity.

The lock may be changed only through a reviewed dependency-update pull request that includes configure, build, compiled-test, and Editor acceptance evidence.

## Resolution order

The Developer Preview coordinator resolves `engine_root` in this order:

1. explicit `--engine-root`;
2. `FOA_O3DE_ROOT`;
3. the sibling directory named by `checkout_directory` in `o3de.lock.json`.

There is no supported combined product/engine route. The build root resolves from explicit `--build-dir`, then `FOA_BUILD_ROOT`, then the sibling `foa-build/` directory.

## Product-owned Gem discovery

The external engine does not own the FOA Gems. Therefore `TaintedGrailModdingEditor/project.json` registers exactly these project-relative external subdirectories:

```text
../Gems/ExternalToolchain
../Gems/TaintedGrailModdingSDK
```

The same project manifest enables both Gem names exactly once. This allows the pinned engine to discover product-owned Gem descriptors without adding FOA paths to upstream `engine.json` or relying on a user-global O3DE manifest.

The standalone project `EngineFinder.cmake` resolves the external engine, reads the lock, and independently rejects a checkout at any other Git commit.

## Configure boundary

CMake configuration is engine-owned while the project is product-owned:

```text
cmake --preset windows-vs-unity
  -S <engine_root>
  -B <build_root>/tg-sdk-developer-preview-0-windows-profile
  -A x64
  -DLY_PROJECTS=<product_root>/TaintedGrailModdingEditor
```

FOA validators execute from `product_root`. The O3DE source-policy validator is resolved from `engine_root` and scans both product Gems. Compiled tests execute from `build_root`.

Clickable-entry policy records all three roots separately. It rejects a CMake cache whose source is FOA-SDK, a build inside either source checkout, or a shortcut that reports the product as its engine.

## Working-tree acceptance gates

Before the extraction pull request can leave draft state, all of the following must pass against the external sibling engine:

1. prerequisite and exact-pin verification;
2. configure from `engine_root`;
3. Profile Editor and AssetProcessorBatch build;
4. all focused Python tests and validators;
5. O3DE source-policy validation of both custom Gems;
6. compiled catalog and framework tests;
7. Editor entry and pane acceptance;
8. generated-output and tracked-path hygiene checks.

The clean-tree contract itself also fails when any forbidden engine root or stock Gem reappears.

## FOA-O3DE decision

Do not create `FOA-O3DE` merely to hold inherited engine content. Create it only if the changed-path audit proves that FOA requires intentional modifications to stock O3DE files and those changes cannot be implemented through the FOA project or custom Gems.

Until such evidence exists, the maintained repositories are expected to be:

```text
FOA-SDK    maintained product repository
o3de       pinned upstream checkout
FOA-O3DE   absent
```

## History extraction and filtered-history migration

The working tree is now FOA-only, but deleting O3DE directories in a normal commit does not remove them from earlier commits or Git object storage. A fresh clone still inherits the old fork history until the history extraction and filtered-history migration are completed.

That later migration must:

1. record the exact accepted pre-filter FOA head;
2. generate a filtered repository containing only the accepted product paths and their relevant history;
3. verify the filtered tip is content-equivalent to the accepted FOA-only tree;
4. preserve or archive tags and references deliberately;
5. publish a migration notice and force-update procedure;
6. re-run clean-clone, validation, external-engine configure/build, and Editor acceptance;
7. replace the default product history only after those checks pass.

The history migration must not be confused with creation of an engine fork. It removes inherited O3DE history from FOA-SDK; it does not move that history into `FOA-O3DE`.
