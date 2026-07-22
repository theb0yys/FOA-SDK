# Source Register

All entries are bound to upstream commit `d7e740e7f167b73152b53409e483dab07d80d048`.

| ID | System | Pinned version/status | Primary source | Intended FOA-SDK utility |
|---|---|---|---|---|
| `TG-PORT-001` | Tainted Framework | `0.1.33` Mono live-load validated for game `1.23.401`; later IL2CPP 0.1.36 package install/load evidence is separately bound by the adapter route | `mods/tainted-framework` | Branch/runtime distinction, framework identity, configuration, diagnostics, registries, golden fixtures |
| `TG-PORT-002` | Tainted Interface | `0.2.6`; Level 3 runtime proof; exact FoA game version not recorded; Level 4 visual proof blocked | `mods/Tainted Interface` | Semantic UI/icon IDs, safe asset resolution, project-owned fallback SVGs, theme tokens, later Qt asset consumption |
| `TG-PORT-003` | Avalon Core | `0.8.4`; planning/authoring core with bounded downstream authority | `mods/avalon-core` | Canonical game knowledge, engine registries, schemas, planning contracts, Road Atlas |
| `TG-PORT-004` | FoA Mod Manager | `0.6.44`; Mono; exact game version unknown until local validation | `mods/foa-mod-manager` | Mod identity/config inventory, UI-scope contract, controller/status metadata, packaging information |
| `TG-PORT-005` | Template Diagnostics | `0.4.55`; Mono assumed | `mods/template-diagnostics` | Read-only capture schemas, offline validators, performance/audio/material/template observations |
| `TG-PORT-006` | Avalon AI Runtime | `0.8.0`; Mono; installed Steam build `23532579`; live behavior gates pending | `mods/avalon-ai-runtime` | Portable V2 contracts, blackboards, planning, package registry, authoring and validation |
| `TG-PORT-007` | Avalon Awakened | `0.6.5`; creator-asset correction; not releasable | `mods/avalon-awakened` | Kandra/Unity conversion evidence and later character/asset authoring routes |
| `TG-PORT-008` | Research taxonomy/catalog | exact commit only | `docs/research`, `docs/foa-modding-environment.md` | Proven vocabulary, source classification, known system evidence, import seed data |
| `TG-PORT-009` | Merlin Workshop | optional contract-only provider; source baseline not yet pinned in this track | separate user fork plus official `AR-Questline/merlin-workshop` | Optional discovery/export route only; never a required or overriding authority |

## Licence and payload rule

The pinned repository has no root `LICENSE`, `COPYING`, or `NOTICE` file. Every imported source fact therefore
retains `NOASSERTION` until the user supplies an applicable licence grant. The current port does not copy the
Tainted Interface asset library, prebuilt DLLs, game assemblies, or generated live outputs.
