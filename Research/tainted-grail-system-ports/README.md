# Tainted Grail System Ports

## Scope

This research track governs selective ports from the user's proven Tainted Grail repository into FOA-SDK.
It does not include, inspect, or depend on any Waning or Bannerlord repository.

The pinned upstream baseline is:

- repository: `theb0yys/Tainted-Grail-The-Fall-of-Avalon-mods`;
- branch: `main`;
- commit: `d7e740e7f167b73152b53409e483dab07d80d048`;
- captured: `2026-07-21`;
- repository licence: `NOASSERTION` because no root licence file exists at the pinned commit.

No upstream asset payload, DLL, game file, BepInEx host, Unity assembly, private path, or live-game output is
copied by this research track. Contracts, reviewed facts, semantic identifiers, deterministic fixtures, and
portable editor logic are ported independently. Runtime behavior remains behind separately reviewed adapters.

The compiled import layer now includes Tainted Framework knowledge, Tainted Interface semantics,
provider-neutral acquisition observations, Road Atlas schema validation, Avalon AI API 2.0 authoring
contracts, and distinct inert Mono/IL2CPP adapter routes. Provider execution, Road/AI durable editor vertical
slices, and executable runtime packages remain later gates rather than hidden authority in these ports.

## Areas

| Area | Purpose |
|---|---|
| [Foundation frameworks](areas/01-foundation-frameworks/README.md) | Tainted Framework and Avalon Core contracts, branch knowledge, registries, and golden fixtures |
| [Interface and manager](areas/02-interface-and-manager/README.md) | Tainted Interface semantic assets/editor styling and FoA Mod Manager integration contracts |
| [Diagnostics and intake](areas/03-diagnostics-and-intake/README.md) | Template Diagnostics, read-only capture, offline validators, and governed evidence submission |
| [Road Atlas](areas/04-road-atlas/README.md) | Typed road inventory, geometry, topology, anchors, rules, and Road Editor inputs |
| [Avalon AI](areas/05-avalon-ai/README.md) | Portable AI contracts, blackboards, planning, authoring, validation, and runtime adapter split |
| [Runtime and acquisition](areas/06-runtime-and-acquisition/README.md) | Local capture, pinned GitHub, optional Merlin, Kandra/Unity routes, Mono and IL2CPP adapters |

See [SOURCE_REGISTER.md](SOURCE_REGISTER.md) for the exact upstream set and [PORT_GATES.md](PORT_GATES.md)
for the ordered implementation boundary.
