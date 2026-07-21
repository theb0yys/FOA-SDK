# Tainted Interface UI Utilities and Approved Embedded Assets

## Status

Implemented as an engine-neutral Framework utility slice. O3DE host compilation, compiled CTest execution, and Windows visual review remain pending.

## Pinned source

The utility contract is derived from Tainted Interface at:

- repository: `theb0yys/Tainted-Grail-The-Fall-of-Avalon-mods`;
- branch: `main`;
- commit: `d7e740e7f167b73152b53409e483dab07d80d048`;
- framework version: `0.2.6`;
- plugin GUID: `kane.tgfoa.tainted-interface`.

Every selected source document and implementation file has an exact Git blob SHA-1 binding in the service.

## Portable utilities

`TaintedInterfaceUi::Service` provides copied, deterministic values for:

- colour and typography/layout tokens;
- the accepted curated semantic asset catalog;
- exact upstream source metadata and dimensions;
- asset-resolution status;
- project-owned embedded fallback bytes and fingerprints;
- safe semantic-ID validation;
- contained HUD-badge rectangle calculation.

The service is owned persistently by `FoundationService`. It exposes no renderer, window, cursor, process, file, deployment, runtime, save, catalog, source-registry, workspace, or profile authority.

## Curated catalog

The catalog contains exactly nineteen semantic IDs:

- twelve common UI texture IDs for HUD, progress, slots, dialog, tabs, checkboxes and divider presentation;
- four UI-action icon IDs;
- three input-device icon IDs.

Consumers use semantic IDs. They do not receive arbitrary source paths, folder-scanning APIs or full-library enumeration.

## Asset approval and licence boundary

The upstream research accepted the semantic catalog and prototype embedding lane. It did not establish a redistributable licence for the selected PNG payload. The SDK therefore records every upstream asset with:

- `license = NOASSERTION`;
- `upstream payload embedded = false`;
- `upstream redistribution allowed = false`.

No upstream PNG, full `Assets/User Interface` tree, ArtStation source, Unity metadata, prefab, SVG, FBX, TGA, WAV or other source payload is copied into the O3DE repository or build graph by this unit.

## Approved embedded assets

This unit embeds only three tiny project-owned SVG fallback classes:

- generic frame/content placeholder;
- generic action/confirmation placeholder;
- generic input-device placeholder.

Each fallback:

- is authored for FOA-SDK;
- is compiled as a bounded UTF-8 constant;
- has an exact recomputed SHA-256 fingerprint;
- is redistributable under the repository licence;
- explicitly does **not** claim Tainted Interface or upstream visual parity.

The nineteen semantic IDs map to one of these three generic classes until an exact upstream asset receives separate provenance, licence, redistribution, build and visual approval.

## Runtime boundary

This unit does not import or invoke:

- BepInEx, UnityEngine or Harmony;
- Tainted Interface plugin startup or teardown;
- IMGUI rendering, cursor capture or custom UI scopes;
- loose asset roots or embedded .NET resource reflection;
- filesystem reads, texture decoding or caching;
- FoA runtime, deployment, launch, save or mutation behavior.

Acquisition routes and separate Mono/IL2CPP runtime adapters remain later reviewed units.

## Validation

The focused validator requires:

- the exact nineteen-ID catalog;
- exact upstream identity and blob bindings;
- `NOASSERTION` and disabled upstream embedding/redistribution for every entry;
- byte-for-byte SHA-256 verification of every project-owned SVG fallback class;
- persistent Foundation ownership;
- production-linked compiled tests;
- local-validation and documentation integration;
- no runtime, filesystem or mutable SDK authority.

The compiled tests cover deterministic ordering, blocked upstream payloads, fallback metadata, semantic-ID refusal, style/metric ordering, HUD layout containment and persistent service ownership.

## Deferred

This unit adds no Editor pane and does not change the Windows pane count. Qt rendering helpers, actual image decode, accessibility review of rendered controls, approved upstream visual assets, acquisition providers, and runtime adapters require independent development units and evidence.
