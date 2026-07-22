# Integration plug-ins

This category contains optional external-tool, acquisition, import/export, and workflow integrations.

## Implemented packages

- [`ApprovedAcquisition`](ApprovedAcquisition/README.md) — deterministic local and exact pinned-GitHub acquisition for reviewed framework knowledge and project-owned fallback assets. It produces externally stored, hash-verified bundles without evidence promotion, runtime, deployment or publication authority.
- [`MerlinsWorkshop`](MerlinsWorkshop/README.md) — optional exact Git/LFS work-order planning and detached-checkout qualification for the official Merlin's Workshop toolkit. It verifies pinned source and licence material without cloning, downloading, launching Unity, building mods or redistributing official payloads.

## Deferred packages

Executable acquisition dispatch remains deferred until the separately reviewed ExternalToolchain execution boundary exists. Mono and IL2CPP remain distinct later runtime-adapter packages and are not part of either integration.

External discovery and process execution must route through `Gems/ExternalToolchain`. Integration packages may not silently download, execute, redistribute, promote, deploy, sign, publish, or mutate content.

Every package directory requires a schema-valid `plugin.json` at its root.
