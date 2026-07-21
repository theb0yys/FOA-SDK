# Tainted Framework canonical intake and knowledge contract

Status: implemented authoring-only intake; runtime hosts remain external and prohibited from the Editor build graph.

## Pinned upstream

The knowledge pack is bound to:

- repository: `theb0yys/Tainted-Grail-The-Fall-of-Avalon-mods`;
- branch: `main`;
- commit: `d7e740e7f167b73152b53409e483dab07d80d048`;
- framework version: `0.1.33`;
- plugin GUID: `kane.tgfoa.tainted-framework`;
- capture time: `2026-07-21T16:58:00Z`.

Selected source paths are recorded with exact Git blob SHA-1 fingerprints in
`Knowledge/TaintedFramework/upstream-intake.json`. The upstream licence could
not be established from a repository licence file during this intake and is
therefore recorded fail-closed as `NOASSERTION`. No redistribution or licence
approval is inferred.

## Canonical observations

The pinned release manifest reports:

- Mono framework `0.1.33` live-load validated against FoA `1.23.401` with
  BepInEx `5.4.23.3`;
- shared BCL-only contracts;
- IL2CPP blocked and not implemented as a supported host;
- no public release archive claim;
- only `runtime-report` consumer-ready, and only for the named
  `mods/Tainted-Diagnostic Tool` diagnostics-only consumer.

These are evidence observations, not SDK compatibility grants. A later upstream
commit or a different branch requires a new intake and regenerated manifest.

## Component classifications

Every selected component is classified as exactly one of:

- `portable_authoring_contract`;
- `portable_fixture`;
- `documentation_evidence`;
- `mono_runtime_only`;
- `il2cpp_runtime_only`;
- `game_linked`;
- `editor_utility_candidate`;
- `deferred`;
- `prohibited_in_editor`.

The complete inventory is in
`Knowledge/TaintedFramework/component-inventory.json`. Mono and IL2CPP hosts,
BepInEx bootstrap, Unity calls, game-linked consumers and runtime mutation paths
remain outside the O3DE Framework and Editor targets.

## Knowledge model and golden fixtures

`knowledge.json` records:

- framework, plugin and service identities;
- exact compatibility observations;
- API-surface readiness;
- ExtensionAPI capabilities;
- configuration and diagnostics vocabulary;
- known limitations and evidence paths.

`golden/fixtures.json` records deterministic expectations for plugin-catalog
ordering, duplicate refusal, known and unknown capabilities, exact Mono/IL2CPP
separation, configuration canonicalization, diagnostics ordering and future
schema rejection. `manifest.json` binds every canonical knowledge file by byte
count and SHA-256.

## ExtensionAPI consumer

Tainted Framework is the first named real consumer of the public
`TaintedGrailModdingSDK::ExtensionAPI`. It registers:

- extension ID `extension.tainted-framework`;
- semantic version `0.1.33`;
- exact supported game version `1.23.401`;
- exact supported branch `Mono`;
- `ReadActiveProfile`, `QueryCatalog` and `SubmitCandidateEvidence`.

The consumer receives sanitized profile copies and bounded catalog-record copies.
Candidate evidence must still bind to an already imported source and pass the
existing SourceEvidenceRegistry checks. The consumer receives no mutable
catalog, workspace, profile or source-registry reference and no automatic
promotion, governance, validation, save, signing, publication, deployment or
runtime authority.

## Validation

The repository validator fails closed on:

- upstream repository, branch, commit or version drift;
- missing or malformed source fingerprints;
- duplicate component identities;
- unknown classifications;
- IL2CPP promotion;
- additional consumer-ready API surfaces;
- capability escalation;
- non-canonical JSON or manifest hash drift;
- runtime-host source entering the Framework build graph;
- mutable authority leaking through the public knowledge header.

Production-linked compiled tests cover pinned identity, Mono/IL2CPP separation,
consumer-ready surface uniqueness, deterministic ExtensionAPI registration,
duplicate refusal and branch-drift authorization refusal.

## Deferred work

This unit does not port runtime code. The next unit may port only reviewed,
engine-neutral Tainted Framework editor-facing services. UI Framework utilities
and reviewed embedded assets follow after that. Local/pinned-GitHub acquisition,
Mono/IL2CPP adapters and exact-install parity remain later units.
