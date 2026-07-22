# Suite wizard

This directory owns the current engine-neutral FOA-SDK catalogue-selection interface and native graphical suite-selection, review, and confirmation host.

The wizard presents reviewed suites and packages, prerequisites, exact versions, provenance, licences, compatibility, disk use, conflicts, planned filesystem changes, elevation, rollback behavior and required acknowledgements before confirmation.

`Catalog/` is the read-only discovery and explicit-selection interface. It enumerates reviewed suite/package manifests, emits a canonical catalogue fingerprint, rejects stale selections, delegates package decisions to the resolver, and returns the existing presentation view-model. Its command-line surface remains available for deterministic inspection and automation.

`Host/` is the current native graphical surface. It imports `Catalog/` directly, displays reviewed suite/package/feature choices, captures explicit package and feature selections, submits compatibility context, and renders the returned resolver/view-model rows. It then collects the exact required acknowledgement set plus caller-supplied identity and UTC time before delegating confirmation creation and verification to `ViewModel/`. The host does not calculate dependency effects, acknowledgement requirements, confirmation fingerprints, or alter resolver/view-model output.

`Resolver/` is the engine-neutral source of truth for package decisions. The wizard submits explicit selections, exclusions, features and compatibility context to the resolver, then displays its canonical plan and diagnostics. UI code may not recalculate dependency closure, version constraints, conflicts, compatibility, path safety, legal state, package ordering, payload collisions or plan fingerprints.

`ViewModel/` is the pure presentation and review-confirmation layer. It verifies the exact resolver plan, derives stable package/payload rows and acknowledgement requirements, and creates and verifies a review-only confirmation bound to both `plan_sha256` and `view_model_sha256`.

The current flow is:

```text
reviewed manifests
    → Catalog discovery (`catalog_sha256`)
    → Host package/feature/context choices
    → explicit review-only selection (`selection_sha256`)
    → Resolver plan (`plan_sha256`)
    → ViewModel presentation (`view_model_sha256`)
    → graphical review display (`result_sha256`)
    → explicit acknowledgement set + caller identity/time
    → verified review-only confirmation (`confirmation_sha256`)
```

Launch the graphical host from the product checkout with:

```powershell
python Installer/SuiteWizard/Host/Source/suite_wizard_host.py `
  --installer-root Installer
```

Wizard code must remain a thin presentation layer over deterministic suite resolution and installer services. It may not silently acquire packages, bypass hash or redistribution review, enable runtime adapters, launch FoA, deploy files, mutate saves, sign artifacts, publish releases, mutate the catalog, or promote evidence.

A valid resolution plan and a valid confirmation remain non-executable. The graphical confirmation is transient and grants no operational authority. Any later persistence, acquisition, or execution layer must be separately designed and must reverify the exact current plan, view-model, acknowledgement set, and confirmation before receiving capability.

Generated binaries, plans, confirmations, receipts and screenshots belong under the external build/evidence root, not in this directory.
