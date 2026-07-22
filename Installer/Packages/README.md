# Installer package definitions

Each reviewed installable package occupies one directory:

```text
Installer/Packages/<Package>/package.json
```

`<Package>` must use a stable portable directory name and `package.json` must conform to `../package.schema.json`.

Package manifests describe reviewed source, compatibility, dependencies, conflicts, capabilities, payload fingerprints, install scope, repair/upgrade/uninstall behavior, workspace preservation, legal state, and permanent authority prohibitions.

Plug-in packages retain their canonical `Plugins/<Category>/<Package>/plugin.json`. An installer package may reference a plug-in package but cannot replace its ExtensionAPI declaration or bypass its validation and compatibility gates.
