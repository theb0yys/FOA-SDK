# Suite Wizard catalogue interface

`Installer/SuiteWizard/Catalog/` is the current read-only catalogue-discovery and selection surface beneath the future graphical Suite Wizard host.

It consumes the reviewed manifests under `Installer/Suites/` and `Installer/Packages/`, validates them through the existing resolver contract, and emits canonical review-only JSON. It does not acquire or copy payloads, install packages, elevate privileges, launch FoA, execute runtime adapters, deploy files, mutate saves, sign artifacts, publish releases, mutate the catalogue, or promote evidence.

## Source

```text
Installer/SuiteWizard/Catalog/
├── README.md
└── Source/
    └── wizard_catalog.py
```

The interface has two operations:

1. `discover` emits all reviewed suites, packages, features, compatibility, provenance, lifecycle, licence, payload totals, policies and an exact `catalog_sha256`.
2. `resolve` accepts an exact catalogue fingerprint, suite identity, compatibility context and explicit package/feature choices. It creates a fingerprinted review-only selection, delegates all package decisions to `Resolver/`, and returns the canonical resolver plan plus the existing `ViewModel/` presentation model.

The expected catalogue fingerprint is mandatory for resolution. If any reviewed suite or package manifest changes after discovery, resolution fails and the user must review the new catalogue.

## Command line

Discover the current reviewed catalogue:

```powershell
python Installer/SuiteWizard/Catalog/Source/wizard_catalog.py discover `
  --installer-root Installer
```

Resolve the Developer Preview defaults plus optional documentation:

```powershell
python Installer/SuiteWizard/Catalog/Source/wizard_catalog.py resolve `
  --installer-root Installer `
  --expected-catalog-sha256 <catalog_sha256-from-discover> `
  --suite-id foa.developer-preview `
  --platform windows `
  --architecture x86_64 `
  --runtime-target editor-only `
  --feature foa.feature.documentation
```

`--select`, `--exclude`, and `--feature` may be repeated. The interface never recalculates dependency closure, compatibility, conflicts, legal state, ordering, path safety, payload collisions or plan fingerprints; those decisions remain resolver-owned.

## Host boundary

A later Qt or native wizard host may render the discovery document, collect explicit choices and display the returned view-model. It must preserve the exact catalogue, selection, plan and view-model fingerprints and may not suppress resolver diagnostics or grant operational authority.

All catalogue, selection and result authority fields remain false. A resolved selection is still review-only and non-executable.
