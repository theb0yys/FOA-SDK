# Tainted Grail: The Fall of Avalon Modding Editor and SDK

An O3DE-based, evidence-governed authoring environment for creating, validating, packaging, and eventually deploying mods for **Tainted Grail: The Fall of Avalon**.

> **Project status: pre-alpha.** The editor foundation is under active development. It is not yet a complete mod-production toolchain and must not be treated as a safe runtime executor.

This repository is a fork of Open 3D Engine. O3DE supplies the editor host, UI framework, build system, asset tooling, reflection, automation, and extensibility. Tainted Grail remains a separate Unity runtime target.

## Project goals

The project is building a public modding editor and SDK that can:

- describe an exact FoA installation and game build;
- create pack-owned mod projects and manifests;
- import and fingerprint research, diagnostics, dumps, observations, logs, screenshots, CSV, JSON, and Avalon Core source sets;
- preserve exact native references, GUIDs, paths, aliases, versions, and evidence;
- maintain a canonical, queryable game-knowledge catalog;
- separate research maturity, validation, risk, and runtime permission;
- author items, recipes, actors, troops, spawns, routes, factions, quests, state, assets, and localisation through shared governed data;
- generate reviewed work orders and manifests for separately validated FoA runtime adapters;
- provide a controlled validate, generate, build, package, deploy, launch, capture, and evidence workflow.

## Safety and architecture boundary

The editor and Avalon Core knowledge layer do **not** execute gameplay actions.

They may:

- manage workspaces and pack ownership;
- import and preserve evidence;
- query and inspect game knowledge;
- validate references and relationships;
- produce blockers, permissions, plans, and handoff documents;
- build editor-owned packages and reports.

They must not directly:

- call FoA game APIs;
- patch the game with Harmony;
- load BepInEx plugins into the game;
- spawn actors or encounters;
- grant items or recipes;
- mutate quests, saves, vendors, loot, or world state;
- claim runtime safety without the required evidence and validation.

Native execution belongs to separate adapters with their own compatibility, persistence, cleanup, and rollback gates.

## Current editor tools

The `TaintedGrailModdingSDK` Gem currently registers these O3DE tools under **Tools → Tainted Grail SDK**:

### Tainted Grail SDK Status

Configures and reports:

- workspace identity and paths;
- exact FoA game profile;
- Mono or IL2CPP target;
- Unity and BepInEx versions;
- pack, source, evidence, catalog, and blocker totals;
- catalog coverage and validation blockers.

Workspace documents use the `*.tgworkspace.json` suffix.

### Tainted Grail Pack Manager

Creates and opens governed `*.tgpack.json` manifests containing:

- stable namespaced pack ID and owner;
- semantic version;
- game, Avalon Core, and adapter compatibility;
- dependencies, required mods, and incompatibilities;
- save-impact declaration;
- content, asset, and localisation declarations;
- build configuration and release channel.

Runtime actions are always persisted as disabled.

### Tainted Grail Source Intake

Imports and fingerprints source artifacts using:

- structured JSON evidence extraction;
- structured CSV evidence extraction;
- generic artifact registration without inferred evidence.

Every source is bound to the active profile ID, exact game version, branch, and runtime target. Durable files are written under:

```text
Sources/<source-id>/source.tgsource.json
Sources/<source-id>/evidence.tgevidence.json
```

### Tainted Grail Catalog Browser

Provides:

- exact native-reference and GUID search;
- search by record ID, subject, alias, domain, kind, pack, confidence, validation, permission, evidence, blocker state, and supersession;
- inspection of identity, pack ownership, evidence, relationships, validation history, conflicts, missing refs, prohibitions, and blockers;
- evidence-backed promotion into reviewed-but-unvalidated canonical records;
- durable `Catalog/catalog.tgcatalog.json` persistence bound to the active workspace and exact game profile.

Catalog records are never merged by display name. Claim promotion cannot grant allowed usage and automatically adds `no_unvalidated_runtime_use`.

## Development roadmap

The active roadmap is maintained in [ROADMAP.md](ROADMAP.md). Major capability areas are:

1. workspace and exact game-profile management;
2. mod and content-pack project management;
3. source and evidence intake;
4. canonical catalog browser and record inspector;
5. validation, maturity, risk, and permission engine;
6. specialised domain authoring tools;
7. typed FoA adapter contracts;
8. build, package, deploy, test, and evidence capture.

## Documentation

Start with the [TG SDK documentation hub](docs/tainted-grail-sdk/README.md).

- [User guide](docs/tainted-grail-sdk/USER_GUIDE.md)
- [Architecture](docs/tainted-grail-sdk/ARCHITECTURE.md)
- [Development guide](docs/tainted-grail-sdk/DEVELOPMENT_GUIDE.md)
- [Code quality standard](docs/tainted-grail-sdk/CODE_QUALITY.md)
- [Review and merge policy](docs/tainted-grail-sdk/REVIEW_AND_MERGE_POLICY.md)
- [Data formats](docs/tainted-grail-sdk/DATA_FORMATS.md)
- [Release process](docs/tainted-grail-sdk/RELEASE_PROCESS.md)
- [Legal and content policy](docs/tainted-grail-sdk/LEGAL_AND_CONTENT_POLICY.md)
- [Privacy and telemetry](docs/tainted-grail-sdk/PRIVACY.md)
- [Accessibility](docs/tainted-grail-sdk/ACCESSIBILITY.md)
- [Glossary](docs/tainted-grail-sdk/GLOSSARY.md)
- [Governance](GOVERNANCE.md)
- [Contributing](CONTRIBUTING.md)
- [Security](SECURITY.md)
- [Support](SUPPORT.md)

## Repository layout

```text
Gems/TaintedGrailModdingSDK/       TG editor/SDK Gem
Gems/TaintedGrailModdingSDK/Code/  C++ editor models, services, and tools
Gems/TaintedGrailModdingSDK/Tools/ Repository contract validation
docs/tainted-grail-sdk/            Public user and contributor documentation
.github/                            Issue forms, pull-request template, ownership
engine.json                         O3DE engine registration
```

The remaining repository content is inherited from O3DE and remains subject to O3DE's build, licence, and contribution requirements.

## Building

This fork uses the standard O3DE source build process. Follow the upstream O3DE source-build requirements for your platform, configure the engine with a supported compiler and third-party package cache, and build the O3DE Editor host.

Before opening a pull request, run:

```shell
python Gems/TaintedGrailModdingSDK/Tools/validate_foundation.py
```

A complete contribution must also pass the repository validation workflow and the applicable O3DE host build jobs.

## Branch and review model

The public project uses two long-lived branches:

- `main` — reviewed, integrated project state;
- `foa-development` — the single active development line.

Direct development on `main` is prohibited. Significant changes require design review before implementation, self-review before push, a pull request, successful required checks, and maintainer approval before merge. See [Review and merge policy](docs/tainted-grail-sdk/REVIEW_AND_MERGE_POLICY.md).

## Contributing

Contributions are welcome. Read [CONTRIBUTING.md](CONTRIBUTING.md) before filing an issue or changing code.

All commits must include Developer Certificate of Origin sign-off:

```shell
git commit -s -m "Describe the change"
```

Contributors must follow the [Code of Conduct](CODE_OF_CONDUCT.md), code-quality rules, evidence governance, and review requirements.

## Security

Do not disclose vulnerabilities, unsafe deployment paths, or save-corruption risks in public issues. Follow [SECURITY.md](SECURITY.md).

## Licence and upstream attribution

This repository retains the O3DE licence files and third-party notices at the repository root. Contributions must be compatible with those terms.

Tainted Grail: The Fall of Avalon and related names, assets, and marks belong to their respective rights holders. This is an independent community project and is not affiliated with or endorsed by the game's rights holders. Do not submit copyrighted game assets or proprietary source material that you do not have permission to distribute.
