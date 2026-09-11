# Local mod package export

## Outcome and ownership

The Mod Package Builder exports editable authoring data and reopens it in a new workspace. capability-execution owns the separate Preview/Export/Inspect/Import lifecycle; Foundation supplies immutable snapshots and remains owner of workspace publication. A built-in local authoring package provider performs bounded file work. UI presents inventory and forwards explicit commands. Existing adapter V1 preview and permission flags remain inert. This does not create a game-loadable runtime mod.

## Contract

Version 1 uses a deterministic UTF-8 JSON archive with extension .tgmod. Its manifest binds the selected pack, exact profile identity/version/branch/runtime, dependency pack versions, ordered relative entry paths, lengths and SHA-256 digests. Entry bytes use canonical base64. The manifest SHA-256 binds the explicit preview to export; export also checks input snapshots for drift. Checksums detect corruption, not authenticity or redistribution permission. Export/import receipts identify the built-in provider, artifact fingerprint and resulting path, without granting downstream permissions.

Bounds: 64 MiB decoded content, 96 MiB archive, 4096 entries, 512 packs and 64 levels of JSON nesting. Paths are relative, ASCII, case-insensitively unique and reject traversal, aliases, device names, drives, separators other than slash and trailing dots/spaces. No general extraction API, executables, compression or external process/dependency is introduced.

## Selection and privacy

Include the selected pack and its transitive declared workspace pack dependencies in deterministic order. Reject missing dependencies, cycles, conflicts, duplicate pack identities, unresolved references and unsupported file declarations. Required third-party runtime mods remain explicit manifest requirements and do not establish runtime compatibility.

Include owned synthetic records and typed economy/population/encounter/society/world/quest/asset/localisation definitions, subordinate links and presentation assignments. Native targets are exact identity anchors, with no native typed payload, original labels or extracted files. Local modifications to native definitions without durable pack ownership are not inferred into this package. Core validates the filtered catalog and reference closure. Source/evidence metadata is limited to needed authoring intent and neutral native-reference declarations; private source locators and source payload files are excluded. Imported records have no allowed usages or governance/validation history, and native anchors remain unverified references.

Images must be managed PNG/JPEG assets; legacy portraits must reference an included managed image, with complete provenance and a reviewed redistribution declaration. Validate actual bytes, decoder limits, fingerprints and paths. Never recursively copy workspace folders. Project text and metadata are checked for non-portable machine paths; failures identify the entry to repair.

## Compatibility and reconstruction

Package version 1 accepts only the current catalog schema 7 and pack/workspace schemas 1. This owner-authorized local authoring batch is additive to the accepted execution-plane direction and does not execute or relax the separately scoped M1 or adapter V1 contracts. Unknown archive versions/fields and malformed entries fail closed. Existing readers and catalogs are unchanged. Reopening requires a locally configured matching game profile and a new destination directory. Reconstruct canonical workspace-relative output folders, pack manifests, selected catalog and needed source/evidence documents; retain stable content IDs and references. Local profile paths come from the user's current configuration, never the package. Save and validate in a private sibling staging directory, verify each write, then publish by non-replacing rename. Existing destinations are rejected. Failure or cancellation cleans only owned temporary paths and preserves source/output state.

## Execution and UI

Preview is explicit, with inventory, dependency list, warnings and actionable blockers. Export consumes the exact preview fingerprint and checks source snapshot/media drift. An atomic non-replacing output commit preserves existing archives. Repeated export of unchanged content is byte-identical. Import verifies the full archive before writing. Long inventory, hashing, encoding and filesystem work run off the Editor thread; UI receives bounded progress and cancellation. No work occurs on every render/tick. Closing a busy pane cancels and waits safely for its own bounded worker.

## Validation

Add compiled tests for selection and native anchors, all typed collections, dependencies/cycles/conflicts, stable ordering, integrity and exact fingerprints; PNG/JPEG and localisation/assignment round trips; private path/protected root/symlink/traversal/case collisions/unknown fields/future versions/corruption/limits; preview drift, cancellation and existing-target preservation. Run pinned-host builds and actual Editor preview/export/import/open and error recovery. Use synthetic owned data; budget maximum UI timer gap 3 seconds and bounded 64 MiB package tests. Logs, artifacts, screenshots and exact-head evidence remain private. Installer, deployment, game runtime and release lanes are not applicable.
