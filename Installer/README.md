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
├── Bootstrapper/       prerequisites, acquisition and verified handoff
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

The wizard must display exact versions, source provenance, licence state, compatibility, required disk space, dependencies, conflicts, elevation requirements, and planned filesystem changes before confirmation. It must support dry-run, repair, upgrade, uninstall, and rollback-aware review.

A selection never grants game-launch, runtime-execution, deployment, save-mutation, signing, publication, catalog-mutation, or evidence-promotion authority.

## Bootstrapper

The bootstrapper owns bounded prerequisite checks and the verified transition into the suite wizard or package engine. It may resolve reviewed package sources only after exact identity, version, hash, signature policy, licence, and compatibility checks pass.

It must fail closed for unavailable prerequisites, hash drift, unsupported operating systems, unknown packages, dependency cycles, unsafe paths, unreviewed redistribution, unexpected elevation, and partial acquisition. Network access and elevation are explicit capabilities, never defaults.

## Suites and packages

Each suite lives at `Installer/Suites/<Suite>/suite.json` and conforms to `suite.schema.json`.

Each installable package lives at `Installer/Packages/<Package>/package.json` and conforms to `package.schema.json`.

Suite definitions reference package IDs; they do not duplicate payload inventories. Package definitions describe reviewed source, compatibility, dependencies, payload ownership, install scope, lifecycle behavior, legal status, and permanent authority prohibitions.

Plug-in package manifests remain under `Plugins/`. Installer package definitions may reference those plug-ins, but do not replace `plugin.json` or bypass ExtensionAPI registration.

## Existing Windows packaging

The current reviewed Windows MSI/portable workflow is owned beneath `Installer/Packaging/Windows/`. It converts an already verified staging payload into an unsigned development MSI or portable ZIP. It does not select unreviewed files, publish releases, sign artifacts, modify FoA, or remove external workspaces.

The installed Editor launcher source is owned beneath `Installer/Launcher/Windows/` and remains built through the required SDK foundation target until the suite bootstrapper has its own reviewed build graph.

## Acceptance boundary

Installer changes require, as applicable:

1. canonical schema validation;
2. deterministic suite and package resolution;
3. dependency/conflict and compatibility tests;
4. path, symlink, case-collision, and traversal rejection;
5. exact inventory, hash, provenance, licence, and redistribution review;
6. clean install, repair, upgrade, rollback, and uninstall smoke tests;
7. preservation of external workspaces and user-authored content;
8. generated-output hygiene;
9. explicit proof that no release, signing, runtime, deployment, or save authority was introduced.
