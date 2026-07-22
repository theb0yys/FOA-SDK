# Suite wizard

This directory owns the future user-facing FOA-SDK suite-selection wizard.

The wizard presents reviewed suites and packages, prerequisites, exact versions, provenance, licences, compatibility, disk use, conflicts, planned filesystem changes, elevation, and rollback behavior before confirmation.

Wizard code must remain a thin presentation layer over deterministic suite resolution and installer services. It may not silently acquire packages, bypass hash or redistribution review, enable runtime adapters, launch FoA, deploy files, mutate saves, sign artifacts, publish releases, mutate the catalog, or promote evidence.

Generated binaries and screenshots belong under the external build/evidence root, not in this directory.
