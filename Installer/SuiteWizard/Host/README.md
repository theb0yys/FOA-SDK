# Graphical Suite Wizard host

`Installer/SuiteWizard/Host/` owns the current native graphical review and confirmation host for the FOA-SDK installer catalogue.

The host imports the engine-neutral `Catalog/` and `ViewModel/` contracts directly. It does not parse suite/package manifests independently and does not reproduce dependency, compatibility, conflict, legal, ordering, payload-collision, acknowledgement, confirmation, or fingerprint logic.

## Launch

From the FOA-SDK checkout:

```powershell
python Installer/SuiteWizard/Host/Source/suite_wizard_host.py `
  --installer-root Installer
```

The host uses Python's standard-library `tkinter` and themed `ttk` widgets. If native Tk support or a graphical display is unavailable, startup fails closed with a diagnostic and performs no fallback mutation.

## User flow

The **Choose** page displays:

- the exact reviewed `catalog_sha256`;
- reviewed suite identity, version, channel, and description;
- compatibility context fields;
- required, default, and optional package rows;
- exact package versions, status, payload totals, and licence expressions;
- reviewed feature groups and their package references.

Required packages are visible and locked. Default packages begin included and may be explicitly excluded. Optional packages begin excluded and may be explicitly selected. Feature choices are submitted as feature IDs; the host does not calculate their dependency effect.

The **Resolve review** action creates the exact review-only selection through `Catalog/`, then asks the existing resolver and view-model layers for the current result. The **Review** page displays:

- `catalog_sha256`;
- `selection_sha256`;
- `plan_sha256`;
- `view_model_sha256`;
- `result_sha256`;
- resolver-owned package order;
- planned destination paths, sizes, and payload hashes;
- warnings;
- required acknowledgement text.

The **Confirm** page renders every required acknowledgement as an explicit checkbox and collects caller-supplied confirming identity plus an ISO-8601 UTC timestamp. The host compares the displayed `plan_sha256` and `view_model_sha256` with the exact current in-memory review before delegating to `ViewModel.create_confirmation()` and `ViewModel.verify_confirmation()`.

A successful confirmation displays:

- the exact bound `plan_sha256`;
- the exact bound `view_model_sha256`;
- `confirmation_sha256`;
- the exact acknowledged ID set;
- caller-supplied identity and UTC time;
- the immutable review-only statement;
- an all-false authority record.

Refreshing the catalogue or changing any suite, package, feature, compatibility context, acknowledgement, confirmer identity, or timestamp invalidates the applicable displayed review or confirmation. Manifest drift after discovery is rejected by the existing stale-catalogue gate.

## Authority boundary

This host is review-only. It has no downloader, network client, package copier, filesystem writer, installer engine, elevation helper, launcher, runtime executor, deployment service, save mutator, signer, publisher, catalogue mutator, or evidence promoter.

The confirmation remains transient and is not written to the repository or installation target. There is no install or execution button. Any later persistence, acquisition, or execution unit must be separately reviewed and must reverify the complete accepted chain before receiving capability.
