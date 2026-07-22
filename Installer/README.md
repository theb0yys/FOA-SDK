# FOA-SDK installer and suite system

`Installer/` is the product-owned source root for installing, repairing, upgrading, and removing the FOA-SDK suite.

It owns installer source and declarative package recipes. It does **not** contain generated MSI files, portable ZIP archives, staged payloads, build caches, receipts, handoffs, screenshots, signing material, or release uploads. Generated output belongs beneath the external `foa-build/` root or another reviewed output directory.

## Layout

```text
Installer/
├── README.md
├── suite.schema.json
├── package.schema.json
├── SuiteWizard/        selectable suite user experience
│   ├── Catalog/        reviewed catalogue discovery and explicit selection
│   ├── Host/           native graphical choice, review, confirmation, and receipt surface
│   ├── Receipt/        deterministic local confirmation-receipt persistence
│   ├── Resolver/       deterministic package decision and dry-run plan layer
│   └── ViewModel/      deterministic presentation and review-confirmation layer
├── Bootstrapper/       prerequisites and verified non-executing package-engine handoff
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

`Installer/SuiteWizard/Catalog/` provides the engine-neutral discovery and selection interface. It emits reviewed suite/package/feature rows with an exact `catalog_sha256`, binds explicit choices into a `selection_sha256`, rejects stale catalogue state, then delegates resolution and presentation to the existing resolver and view-model contracts.

`Installer/SuiteWizard/Host/` provides the current native graphical review, confirmation, and receipt host. It renders the exact catalogue fingerprint, suite/package/feature rows and compatibility context, translates user controls into explicit selection inputs, and displays the resolver-owned package order, planned files, warnings, acknowledgements and complete review fingerprint chain. Its Confirm page collects the full required acknowledgement set plus caller-supplied identity and UTC time, compares the displayed `plan_sha256` and `view_model_sha256` with the exact current review, then delegates confirmation creation and verification to the existing ViewModel contract. Its Receipt page exports or verifies only the exact current confirmation through the reviewed Receipt contract and displays the complete `plan_sha256` → `view_model_sha256` → `confirmation_sha256` → `receipt_sha256` chain.

The wizard does not resolve packages itself. It submits explicit selections, exclusions, features, and compatibility context to `Installer/SuiteWizard/Resolver/`, then displays the returned canonical plan and diagnostics without weakening them. Dependency closure, version constraints, compatibility, conflicts, path safety, legal state, deterministic ordering, payload collision detection, and plan fingerprinting remain resolver-owned logic.

`Installer/SuiteWizard/ViewModel/` verifies the resolver plan, derives stable UI rows and required acknowledgements, and creates a review-only confirmation bound to exact `plan_sha256` and `view_model_sha256` values. Any catalogue, selection, plan, display-model, acknowledgement, confirmer identity, timestamp, or confirmation mutation invalidates the current review chain.

`Installer/SuiteWizard/Receipt/` re-verifies the exact plan, view-model and confirmation before any persistence. It emits a self-contained canonical receipt with `receipt_sha256`, publishes only to an explicitly selected existing external directory, rejects symbolic-link paths and noncanonical bytes, accepts a byte-identical existing receipt idempotently, and never overwrites a different file. The graphical host delegates to this contract and does not implement a second writer.

`Installer/Bootstrapper/ExecutionHandoff/` re-verifies the complete receipt before deriving one canonical lifecycle request. It binds the requested operation, logical target and prior-installation reference to the exact receipt and records only the capability names a future package engine would require. `granted_capabilities` remains empty and every authority remains false.

A valid resolution plan, confirmation, receipt, and execution handoff are still non-executable. Receipt and handoff publication are local evidence persistence, not acquisition, installation, deployment, or elevation. A separately reviewed package engine must later reverify the exact accepted chain and receive explicit capability through a separate trusted channel.

A selection never grants game-launch, runtime-execution, deployment, save-mutation, signing, publication, catalog-mutation, or evidence-promotion authority. A confirmation, receipt, or execution handoff also grants no acquisition, installation, elevation, or any of those operational authorities.

## Bootstrapper

The bootstrapper owns bounded prerequisite checks and the verified transition into the suite wizard or package engine. It may resolve reviewed package sources only after exact identity, version, hash, signature policy, licence, and compatibility checks pass.

The receipt-to-execution handoff accepts only canonical receipt bytes, re-verifies the entire embedded chain, derives lifecycle support from every selected package, and emits a logical request for install, repair, upgrade, rollback, or uninstall. Non-install operations require a stable prior-installation reference. Filesystem paths, capability tokens, environment state, and implicit clock values are forbidden.

It must fail closed for unavailable prerequisites, hash drift, unsupported operating systems, unknown packages, dependency cycles, unsafe paths, unreviewed redistribution, unexpected elevation, partial acquisition, stale receipts, unsupported lifecycle operations, logical-target drift, and capability escalation. Network access, process execution, package mutation, and elevation are explicit capabilities, never defaults.

## Suites and packages

Each suite lives at `Installer/Suites/<Suite>/suite.json` and conforms to `suite.schema.json`.

Each installable package lives at `Installer/Packages/<Package>/package.json` and conforms to `package.schema.json`.

Suite definitions reference package IDs; they do not duplicate payload inventories. Package definitions describe reviewed source, compatibility, dependencies, payload ownership, install scope, lifecycle behavior, legal status, and permanent authority prohibitions.

Plug-in package manifests remain under `Plugins/`. Installer package definitions may reference those plug-ins, but do not replace `plugin.json` or bypass ExtensionAPI registration.

## Deterministic resolver

The engine-neutral resolver lives at `Installer/SuiteWizard/Resolver/`. Its public entry point is `Source/suite_package_resolver.py`, and its canonical output conforms to `plan.schema.json`.

For identical suite bytes, package-manifest bytes, explicit selections and compatibility context, the resolver emits byte-identical canonical JSON and the same `plan_sha256`. It includes required/default/optional selection decisions, dependency reasons, dependency-first package order, exact manifest fingerprints, payload inventory, total file count and size, elevation state, legal state, warnings and permanent authority boundaries.

The resolver performs no acquisition, file copying, installation, repair, upgrade, rollback, uninstall, elevation, game launch, runtime execution, deployment, save mutation, signing or publication.

## Wizard view-model, confirmation, receipt, and execution handoff

The engine-neutral view-model contract lives at `Installer/SuiteWizard/ViewModel/`. It emits canonical JSON conforming to `view-model.schema.json`, including exact package rows, flattened payload rows, warnings, policies, totals, acknowledgement requirements and `view_model_sha256`.

`confirmation.schema.json` defines a review-only record bound to the exact plan and view-model hashes, the complete acknowledgement set, caller-supplied identity and caller-supplied UTC time. Confirmation generation reads no clock or environment state and grants no execution authority.

`Installer/SuiteWizard/Receipt/receipt.schema.json` defines the self-contained canonical persistence bundle. Receipt verification reconstructs and verifies the complete embedded plan/view-model/confirmation chain before accepting `receipt_sha256`.

`Installer/Bootstrapper/ExecutionHandoff/request.schema.json` defines the inert package-engine request. The handoff embeds the exact receipt, records required capability names and an empty grant set, and binds the complete chain through `handoff_sha256`.

## Existing Windows packaging

The current reviewed Windows MSI/portable workflow is owned beneath `Installer/Packaging/Windows/`. It converts an already verified staging payload into an unsigned development MSI or portable ZIP. It does not select unreviewed files, publish releases, sign artifacts, modify FoA, or remove external workspaces.

The installed Editor launcher source is owned beneath `Installer/Launcher/Windows/` and remains built through the required SDK foundation target until the suite bootstrapper has its own reviewed build graph.

## Acceptance boundary

Installer changes require, as applicable:

1. canonical schema validation;
2. deterministic suite and package resolution;
3. deterministic catalogue discovery, stale-selection rejection and exact selection fingerprints;
4. graphical host coverage for required/default/optional controls, refresh invalidation and resolver-backed review rows;
5. graphical acknowledgement coverage, exact plan/view-model binding, deterministic confirmation creation and confirmation invalidation;
6. deterministic view-model and exact-hash confirmation;
7. deterministic canonical receipt derivation, atomic create-once persistence, idempotency and exact-chain re-verification;
8. graphical receipt export/verification coverage, exact displayed hash binding, cancellation safety and receipt invalidation;
9. deterministic receipt-to-package-engine handoff derivation, lifecycle support checks, logical-reference validation, empty capability grants, atomic publication and exact-current receipt verification;
10. dependency/conflict and compatibility tests;
11. path, symlink, case-collision, and traversal rejection;
12. exact inventory, hash, provenance, licence, and redistribution review;
13. clean install, repair, upgrade, rollback, and uninstall smoke tests;
14. preservation of external workspaces and user-authored content;
15. generated-output hygiene;
16. explicit proof that no release, signing, runtime, deployment, save, acquisition, installation, elevation or network-publication authority was introduced by review-only or handoff contracts.
