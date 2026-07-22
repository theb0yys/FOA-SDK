# Authoring plug-ins

This category contains optional editor-side authoring systems.

Avalon AI and Road Atlas are available as independently selectable O3DE Tool Gems. Each package registers through the governed cross-module ExtensionAPI request bus and uses host-owned document operations rather than reaching into mutable catalog or workspace internals. Reusable Tainted Interface utilities remain Foundation-owned until an independently licensed payload package exists.

Authoring packages may produce candidate evidence, validation results, typed documents, plans, and reviewed handoffs. They do not gain FoA runtime, deployment, save, signing, publication, or automatic evidence-promotion authority.

Every package directory requires a schema-valid `plugin.json` at its root.
