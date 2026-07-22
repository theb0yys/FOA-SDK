# Integration plug-ins

This category contains optional external-tool, acquisition, import/export, and workflow integrations.

## Implemented packages

- [`ApprovedAcquisition`](ApprovedAcquisition/README.md) — deterministic local and exact pinned-GitHub acquisition for reviewed framework knowledge and project-owned fallback assets. It produces externally stored, hash-verified bundles without evidence promotion, runtime, deployment or publication authority.

## Deferred packages

Merlin's Workshop belongs here as a later optional integration with the official Tainted Grail tool. It remains separate from `ApprovedAcquisition` and requires its own provenance, compatibility, process, acquisition and evidence contracts.

External discovery and process execution must route through `Gems/ExternalToolchain`. Integration packages may not silently download, execute, redistribute, promote, deploy, sign, publish, or mutate content.

Every package directory requires a schema-valid `plugin.json` at its root.
