# FOA-SDK installer and suite system

`Installer/` is the product-owned source root for installing, repairing, upgrading, and removing the FOA-SDK suite.

It owns installer source and declarative package recipes. It does **not** contain generated MSI files, portable ZIP archives, staged payloads, build caches, receipts, screenshots, signing material, or release uploads. Generated output belongs beneath the external `foa-build/` root or another reviewed output directory.

## Layout

```text
Installer/
├── README.md
├── suite.schema.json
├── package.schema.json
├── SuiteWizard/        selectable suite user experience
│   ├── Resolver/       deterministic package decision and dry-run plan layer
│   └── ViewModel/      deterministic presentation and review-confirmation layer
├── Bootstrapper/       prerequisites, acquisition and verified handoff
│   └── Acquisition/    capability-gated approved-acquisition request/result layer
├── Suites/             reviewed suite definitions
├── Packages/           reviewed installable-package definitions
├── Launcher/           installed product launcher source
├── Packaging/          MSI and portable-package build projects
├── Assets/             product-owned installer artwork and localisation
└── Tests/              installer-specific contract and lifecycle tests
```

No additional top-level `Assets/`, `Packages/`, `Tests/`, `Schemas/`, `Build/`, or `Tools/` roots are required. Those materials remain beneath the product subsystem that owns them.

## Suite wizard

The suite wizard is the user-facing selection and review surface. It may present reviewed choices such as:

- the FOA-SDK Editor and mandatory foundation;
- optional authoring plug-ins;
- optional tool integrations;
- optional Mono or IL2CPP runtime-adapter packages;
- documentation, examples, and developer components.

The wizard must display exact versions, source provenance, licence state, compatibility, required disk space, dependencies, conflicts, elevation requirements, planned filesystem changes and required acknowledgements before confirmation. It must support dry-run, repair, upgrade, uninstall, and rollback-aware review.

The wizard does not resolve packages itself. It submits explicit selections, exclusions, features, and compatibility context to `Installer/SuiteWizard/Resolver/`, then displays the returned canonical plan and diagnostics without weakening them. Dependency closure, version constraints, compatibility, conflicts, path safety, legal state, deterministic ordering, payload collision detection, and plan fingerprinting remain resolver-owned logic.

`Installer/SuiteWizard/ViewModel/` verifies the resolver plan, derives stable UI rows and required acknowledgements, and creates a review-only confirmation bound to exact `plan_sha256` and `view_model_sha256` values. Any plan, display-model, acknowledgement or confirmation mutation invalidates the record.

A valid resolution plan and confirmation are still non-executable. A separately reviewed acquisition or execution layer must later reverify both artifacts before receiving any operational capability.

A selection never grants game-launch, runtime-execution, deployment, save-mutation, signing, publication, catalog-mutation, or evidence-promotion authority. A confirmation also grants no acquisition, installation, elevation, or any of those operational authorities.

## Bootstrapper

The bootstrapper owns bounded prerequisite checks and the verified transition into the suite wizard or package engine. It may resolve reviewed package sources only after exact identity, version, hash, signature policy, licence, and compatibility checks pass.

It must fail closed for unavailable prerequisites, hash drift, unsupported operating systems, unknown packages, dependency cycles, unsafe paths, unreviewed redistribution, unexpected elevation, and partial acquisition. Network access and elevation are explicit capabilities, never defaults.

`Installer/Bootstrapper/Acquisition/` is the first capability-gated operational handoff. It reverifies the exact resolver plan, view-model, review confirmation, and approved-provider plan; requires explicit package capability bindings; and emits an acquisition-only request before invoking the existing approved provider.

The local route receives acquisition and external filesystem-write capability only. The pinned-GitHub route additionally requires the reviewed suite network policy and receives network capability only for that exact route. Both routes publish to a new external bundle through the provider's existing exclusive-write, hash-verification, and atomic-rename boundary.

The resulting record proves only that the approved bundle was acquired and reverified. It serializes a logical output reference rather than a private filesystem path, creates no candidate evidence, performs no installation or elevation, and retains no operational authority.

## Suites and packages

Each suite lives at `Installer/Suites/<Suite>/suite.json` and conforms to `suite.schema.json`.

Each installable package lives at `Installer/Packages/<Package>/package.json` and conforms to `package.schema.json`.

Suite definitions reference package IDs; they do not duplicate payload inventories. Package definitions describe reviewed source, compatibility, dependencies, payload ownership, install scope, lifecycle behavior, legal status, and permanent authority prohibitions.

Plug-in package manifests remain under `Plugins/`. Installer package definitions may reference those plug-ins, but do not replace `plugin.json` or bypass ExtensionAPI registration.

## Deterministic resolver

The engine-neutral resolver lives at `Installer/SuiteWizard/Resolver/`. Its public entry point is `Source/suite_package_resolver.py`, and its canonical output conforms to `plan.schema.json`.

For identical suite bytes, package-manifest bytes, explicit selections and compatibility context, the resolver emits byte-identical canonical JSON and the same `plan_sha256`. It includes required/default/optional selection decisions, dependency reasons, dependency-first package order, exact manifest fingerprints, payload inventory, total file count and size, elevation state, legal state, warnings and permanent authority boundaries.

The resolver performs no acquisition, file copying, installation, repair, upgrade, rollback, uninstall, elevation, game launch, runtime execution, deployment, save mutation, signing or publication.

## Wizard view-model and confirmation

The engine-neutral view-model contract lives at `Installer/SuiteWizard/ViewModel/`. It emits canonical JSON conforming to `view-model.schema.json`, including exact package rows, flattened payload rows, warnings, policies, totals, acknowledgement requirements and `view_model_sha256`.

`confirmation.schema.json` defines a review-only record bound to the exact plan and view-model hashes, the complete acknowledgement set, caller-supplied identity and caller-supplied UTC time. Confirmation generation reads no clock or environment state and grants no execution authority.

## Approved-acquisition request and result

The bootstrapper acquisition request is bound to the exact `plan_sha256`, `view_model_sha256`, `confirmation_sha256`, approved provider `plan_id`, route, explicit installer/provider package bindings, named authorizer, UTC authorization time, logical output reference, and `request_sha256`.

Provider bindings use `acquisition.approved.<provider-package-id>` capabilities. The provider package set must match those declarations exactly; display names and path guesses are never used as mappings.

Execution delegates only to `Plugins/Integrations/ApprovedAcquisition`, then verifies the completed bundle again. The canonical result binds the provider receipt, enforces authorization/capture/completion chronology, records acquisition as the only performed effect, and leaves every authority false.

## Existing Windows packaging

The current reviewed Windows MSI/portable workflow is owned beneath `Installer/Packaging/Windows/`. It converts an already verified staging payload into an unsigned development MSI or portable ZIP. It does not select unreviewed files, publish releases, sign artifacts, modify FoA, or remove external workspaces.

The installed Editor launcher source is owned beneath `Installer/Launcher/Windows/` and remains built through the required SDK foundation target until the suite bootstrapper has its own reviewed build graph.

## Acceptance boundary

Installer changes require, as applicable:

1. canonical schema validation;
2. deterministic suite and package resolution;
3. deterministic view-model and exact-hash confirmation;
4. exact confirmation/provider-plan acquisition authorization;
5. dependency/conflict and compatibility tests;
6. path, symlink, case-collision, and traversal rejection;
7. exact inventory, hash, provenance, licence, and redistribution review;
8. verified acquisition bundle and receipt binding;
9. clean install, repair, upgrade, rollback, and uninstall smoke tests;
10. preservation of external workspaces and user-authored content;
11. generated-output hygiene;
12. explicit proof that no release, signing, runtime, deployment, save, installation or elevation authority was introduced by the acquisition slice.
