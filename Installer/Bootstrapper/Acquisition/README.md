# Bootstrapper approved-acquisition handoff

`Installer/Bootstrapper/Acquisition/` is the first capability-gated bridge between a reviewed Suite Wizard confirmation and the existing approved local or exact pinned-GitHub acquisition provider.

It does not select packages, recalculate the suite plan, or weaken provider validation. It reverifies the exact resolver plan, derived view-model, review confirmation, approved provider plan, explicit installer/provider package bindings, reviewed network policy, and caller-supplied authorization chronology.

Installer packages declare approved acquisition through `acquisition.approved.<provider-package-id>` capabilities. The provider plan package set and capability-derived set must match exactly. Names and display labels are never implicit mappings.

`request.schema.json` grants only acquisition, required external filesystem writes, and route-specific network access. Installation, elevation, launch, runtime execution, deployment, save mutation, signing, publication, catalog mutation, and evidence promotion remain false.

`Source/bootstrapper_acquisition.py` invokes only `Plugins/Integrations/ApprovedAcquisition`. The provider retains source identity, containment, pinned URL, byte-limit, SHA-256, exclusive-write, atomic-publication, and bundle-verification ownership.

The result enforces `authorization <= capture <= completion`, binds the verified provider receipt, records acquisition as the only effect, serializes a logical output reference rather than a private path, creates no candidate evidence, and retains no authority.

This unit cannot install, repair, upgrade, roll back, uninstall, elevate, invoke arbitrary providers, acquire Merlin or runtime binaries, launch FoA, deploy, mutate saves, sign, upload, publish, mutate the catalog, or promote evidence.
