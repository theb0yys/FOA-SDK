# FoA Adapter Build Manifest Contract

## Status

Implemented as Correction Slice 11, the first Phase 8 contract slice. It defines a reproducible build definition and a read-only Editor preview for one exact canonical adapter work-order plan.

`BuildAllowed` remains `false`. This slice does not invoke a compiler, assemble a package, copy files, deploy content, launch FoA, or execute an adapter.

## Purpose

Slices 8–10 establish typed adapter declarations, deterministic work-order plans, and runtime-result evidence-return contracts. Slice 11 defines the build handoff that must exist before any later builder is allowed to claim it knows what to compile or package.

The manifest answers:

> Are the exact plan, source revision, O3DE revision, toolchain, dependencies, resolved materials, BepInEx plugin metadata, output layout, fingerprints, and redistribution decisions complete enough to describe a reproducible build?

A `ready` answer means the definition is complete. It does not mean a build has run or that any output exists.

## Research basis

The contract uses a SLSA-inspired separation between the build definition and its resolved materials. It does not claim SLSA compliance or generate provenance in this slice.

BepInEx plugin identity is recorded explicitly through:

- stable plugin GUID;
- human-readable plugin name;
- strict plugin semantic version;
- hard and soft dependencies with exact plugin IDs and versions;
- a package root beneath `BepInEx/plugins/`.

FoA compatibility remains bound to the exact reviewed game profile, branch, runtime target, Unity version, and BepInEx version. A manifest cannot reinterpret a plan for another profile or branch.

## Exact plan binding

Every manifest binds to one generated `AdapterWorkOrderPlan` through:

- plan ID;
- canonical plan JSON;
- lowercase SHA-256 plan fingerprint;
- pack ID and version;
- adapter ID and version;
- pack-required adapter version;
- profile ID;
- game version;
- branch;
- runtime target.

The plan and every step must retain `ExecutionAllowed: false`. Any mismatch produces `plan_mismatch` before toolchain or material checks.

## Build definition

The build definition records:

- stable builder ID;
- strict builder semantic version;
- exact lowercase 40-character source commit;
- exact lowercase 40-character O3DE revision;
- build configuration;
- target framework;
- compiler ID and version;
- deterministic-build declaration;
- continuous-integration build normalization declaration;
- source path-map declaration.

Missing or malformed values produce `toolchain_unresolved`.

The current contract records the toolchain. It does not install, locate, invoke, or verify the compiler.

## Resolved materials

Required material roles are:

- `work_order_plan`;
- `source_tree`;
- `dependency_lock`;
- `toolchain_lock`;
- `license`.

Every material records:

- stable material ID;
- role;
- safe relative locator;
- media type;
- lowercase `sha256:<64 hex>` fingerprint;
- whether it is required;
- whether it would enter the package;
- whether it is legally redistributable.

The `work_order_plan` material must carry the exact plan fingerprint. Missing roles produce `input_missing`; missing or malformed fingerprints produce `fingerprint_missing`.

A source tree, work-order plan, proprietary game file, or other non-redistributable input may be used as a build material without entering the package. A material marked for package inclusion must also be marked redistributable.

## Dependencies

Dependencies use exact stable plugin IDs, strict semantic versions, and typed kinds:

- `hard` — the produced plugin cannot load correctly without the dependency;
- `soft` — the produced plugin can load without it but may use it when present.

Dependency IDs must be unique. The manifest does not download or resolve dependency binaries.

## Expected package outputs

Required expected-output roles are:

- `plugin_binary`;
- `readme`;
- `changelog`;
- `manifest`;
- `license`.

Every output has a safe relative path beneath the package root, a role, a media type, and an explicit redistribution decision. Duplicate output paths, absolute paths, traversal, or output outside the package root produce `path_invalid`.

An expected output that is not redistributable produces `redistribution_blocked`.

Expected outputs are declarations only. No output file, archive, digest, or software bill of materials is created in Slice 11.

## Status precedence

The manifest reports exactly one deterministic status:

1. `plan_mismatch`;
2. `toolchain_unresolved`;
3. `input_missing`;
4. `fingerprint_missing`;
5. `path_invalid`;
6. `redistribution_blocked`;
7. `ready`.

This order prevents a later packaging detail from hiding a more fundamental plan or toolchain mismatch.

## Canonical representation

The manifest produces canonical JSON with:

- fixed property order;
- deterministic dependency, material, output, and reason ordering;
- JSON escaping;
- locale-independent integer formatting;
- exact build-definition and resolved-material fields;
- `BuildAllowed: false`.

Equivalent inputs produce byte-identical canonical JSON. The canonical JSON is transient preview data and is not written to the workspace.

## Read-only Editor pane

**Tainted Grail Adapter Build Manifests** displays:

- exact manifest, pack, adapter, plan, profile, and runtime identities;
- status and reasons;
- builder and toolchain declaration;
- BepInEx plugin metadata;
- resolved materials and fingerprints;
- expected package outputs;
- canonical JSON.

The pane is non-editable. With the ordinary Developer Preview fixture there is no transient adapter declaration, so there are no generated plans and therefore zero ready build definitions.

## No compilation, packaging, deployment, or execution

Slice 11 adds no compiler process, MSBuild or `dotnet build` invocation, file copy, archive creation, dependency download, package assembly, deployment, FoA launch, BepInEx loading, Harmony patching, telemetry, save mutation, or adapter execution.

The contract does not create a durable build-manifest schema. A future builder must be separately reviewed and must consume only an accepted exact manifest.

## Tests and enforcement

Production-linked tests cover strict vocabularies, complete definitions with `BuildAllowed: false`, status precedence, unresolved toolchains, distinct material/fingerprint failures, path traversal and duplicate-output rejection, redistribution blocking, deterministic serialization, input non-mutation, and BepInEx metadata preservation.

The focused validator rejects build execution, runtime/process access, persistence, mutable Editor controls, missing containment and redistribution gates, incomplete tests, and missing public documentation.

## Slice 12 package-assembly preview

Slice 12 implements the next bounded contract as a deterministic **package-assembly preview**. It compares this exact reviewed build manifest with a project-owned staging inventory and derives a sorted package layout, exact output digests, omissions, collisions, trust failures, and redistribution blockers.

The preview requires exact manifest ID/fingerprint, pack, package-root, accepted-review, role, media-type, project-ownership, path, digest, and redistribution bindings. It does not infer missing outputs or choose a winner when multiple staged entries target the same package path.

The preview retains `AssemblyAllowed: false`, `ArchiveAllowed: false`, and `DeploymentAllowed: false`. It performs **no file copying**, filesystem scan, archive creation, deployment, launch, or adapter execution. See [FoA Adapter Package Assembly Preview](FOA_ADAPTER_PACKAGE_ASSEMBLY_PREVIEW.md).

## Rollback

Revert the implementing pull request. Build manifests and package previews are transient derived state, and no durable workspace, pack, catalog, source/evidence, adapter, work-order, runtime-result, build, or package schema requires migration.
