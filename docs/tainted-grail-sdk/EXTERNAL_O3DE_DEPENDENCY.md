# External O3DE dependency boundary

Status: extraction preparation contract.

FOA-SDK is the product. O3DE is the authoring engine dependency. The supported
layout separates three roots:

```text
Development/
├── FOA-SDK/      product_root
├── o3de/         engine_root
└── foa-build/    build_root
```

The roots must not contain one another. Generated build, validation, diagnostics,
screenshot, installer, and release outputs do not belong in either source
checkout.

## Exact engine lock

`o3de.lock.json` is the machine-readable engine contract. The initial reviewed
pin is:

```text
repository: https://github.com/o3de/o3de.git
commit: 68683f23fb747380d3efa2424bd5f30242e9c5a2
engine: o3de
version: 2.7.0
```

The commit is the upstream O3DE commit titled `O3DE Fetch Content (#19622)`.
Developer Preview commands fail closed when the checkout does not resolve to
that exact commit or when `engine.json` reports a different engine identity.

The lock may be changed only through a reviewed dependency-update pull request
that includes configure, build, compiled-test, and Editor acceptance evidence.

## Resolution order

The Developer Preview coordinator resolves `engine_root` in this order:

1. explicit `--engine-root`;
2. `FOA_O3DE_ROOT`;
3. the sibling directory named by `checkout_directory`;
4. legacy combined-tree detection for a migration diagnostic.

The fourth route does not authorise configure, build, or validation and does not
bypass exact-commit enforcement. Because the inherited product checkout has a
FOA commit at `HEAD`, the pin check directs the developer to the required
external O3DE checkout. Combined-tree detection is removed after the FOA-only
repository passes its clean-clone acceptance gate.

The build root resolves from explicit `--build-dir`, then `FOA_BUILD_ROOT`, then
the sibling `foa-build/` directory.

## Product-owned Gem discovery

The external engine does not own the FOA Gems. Therefore
`TaintedGrailModdingEditor/project.json` registers exactly these project-relative
external subdirectories:

```text
../Gems/ExternalToolchain
../Gems/TaintedGrailModdingSDK
```

The same project manifest enables both Gem names exactly once. This allows the
pinned engine to discover the product-owned Gem descriptors without adding FOA
paths to upstream `engine.json` or relying on a user-global O3DE manifest.

The standalone project `EngineFinder.cmake` resolves `FOA_O3DE_ROOT` first and
then the sibling directory named by the lock. It no longer treats FOA-SDK as the
owning engine tree.

## Configure boundary

CMake configuration is engine-owned while the project is product-owned:

```text
cmake --preset windows-vs-unity
  -S <engine_root>
  -B <build_root>/tg-sdk-developer-preview-0-windows-profile
  -A x64
  -DLY_PROJECTS=<product_root>/TaintedGrailModdingEditor
```

FOA validators execute from `product_root`. The O3DE source-policy validator is
resolved from `engine_root`. The compiled tests execute from `build_root`.

## Extraction gates

The repository history extraction is not part of this contract change. It may
begin only after all of the following pass against the external sibling engine:

1. prerequisite and exact-pin verification;
2. configure from `engine_root`;
3. Profile Editor and AssetProcessorBatch build;
4. all focused Python validators;
5. O3DE source-policy validation of both custom Gems;
6. compiled catalog and framework tests;
7. Editor entry and pane acceptance;
8. generated-output hygiene checks.

The extraction must preserve current FOA paths first. Directory reorganisation
comes later.

## FOA-O3DE decision

Do not create `FOA-O3DE` merely to hold inherited engine content. Create it only
if the changed-path audit proves that FOA requires intentional modifications to
stock O3DE files and those changes cannot be implemented through the FOA
project or custom Gems.

Until such evidence exists, the expected maintained repositories are:

```text
FOA-SDK    maintained product repository
o3de       pinned upstream checkout
FOA-O3DE   absent
```

## History extraction

Deleting O3DE directories in a normal commit is not sufficient because the
engine remains in clone history and repository objects. The later history
extraction unit must use a reviewed filtered-history procedure, verify the
result against the recorded source head, preserve tags or archival references
deliberately, and publish a migration notice before replacing the default
product history.
