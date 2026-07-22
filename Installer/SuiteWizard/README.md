# Suite wizard

This directory owns the current engine-neutral FOA-SDK catalogue-selection interface and the future graphical suite-selection wizard host.

The wizard presents reviewed suites and packages, prerequisites, exact versions, provenance, licences, compatibility, disk use, conflicts, planned filesystem changes, elevation, rollback behavior and required acknowledgements before confirmation.

`Catalog/` is the read-only discovery and explicit-selection interface. It enumerates reviewed suite/package manifests, emits a canonical catalogue fingerprint, rejects stale selections, delegates package decisions to the resolver, and returns the existing presentation view-model. Its command-line surface is usable now; a later Qt or native host may render the same contracts without reimplementing them.

`Resolver/` is the engine-neutral source of truth for package decisions. The wizard submits explicit selections, exclusions, features and compatibility context to the resolver, then displays its canonical plan and diagnostics. UI code may not recalculate dependency closure, version constraints, conflicts, compatibility, path safety, legal state, package ordering, payload collisions or plan fingerprints.

`ViewModel/` is the pure presentation and review-confirmation layer. It verifies the exact resolver plan, derives stable package/payload rows and acknowledgement requirements, and creates a review-only confirmation bound to both `plan_sha256` and `view_model_sha256`.

The current flow is:

```text
reviewed manifests
    → Catalog discovery (`catalog_sha256`)
    → explicit review-only selection (`selection_sha256`)
    → Resolver plan (`plan_sha256`)
    → ViewModel presentation (`view_model_sha256`)
    → review confirmation
```

Wizard code must remain a thin presentation layer over deterministic suite resolution and installer services. It may not silently acquire packages, bypass hash or redistribution review, enable runtime adapters, launch FoA, deploy files, mutate saves, sign artifacts, publish releases, mutate the catalog, or promote evidence.

A valid resolution plan and a valid confirmation remain non-executable. Any later acquisition or execution layer must be separately designed and must reverify the exact current plan and confirmation before receiving capability.

Generated binaries, plans, confirmations, receipts and screenshots belong under the external build/evidence root, not in this directory.
