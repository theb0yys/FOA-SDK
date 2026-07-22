# Installer package definitions

Each reviewed installable package occupies one directory:

```text
Installer/Packages/<Package>/package.json
```

`<Package>` must use a stable portable directory name and `package.json` must conform to `../package.schema.json`.

Package manifests describe reviewed source, compatibility, dependencies, conflicts, capabilities, payload fingerprints, install scope, repair/upgrade/uninstall behavior, workspace preservation, legal state, and permanent authority prohibitions. Every installable package must declare at least one exact payload file; a zero-file package is invalid and cannot appear in a resolved installation plan.

Plug-in packages retain their canonical `Plugins/<Category>/<Package>/plugin.json`. An installer package may reference a plug-in package but cannot replace its ExtensionAPI declaration or bypass its validation and compatibility gates.

## Current reviewed packages

The `DeveloperPreview` suite currently resolves three descriptor-level packages:

- `foa.foundation-descriptor` — required pinned `TaintedGrailModdingSDK/gem.json` descriptor;
- `foa.editor-descriptor` — default pinned `TaintedGrailModdingEditor/project.json` descriptor, dependent on the foundation descriptor;
- `foa.installer-documentation` — optional pinned suite-contract documentation.

Each payload row is bound to the reviewed source commit, exact repository-relative source path, destination, byte count, SHA-256 and project-owned redistribution state. These preview packages make the production catalogue displayable and resolvable without claiming that the complete compiled product is installable yet.
