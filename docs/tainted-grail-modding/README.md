# Tainted Grail Modding Knowledge Hub

This directory is the canonical public handbook for creating, validating, packaging, maintaining, and understanding mods for **Tainted Grail: The Fall of Avalon** with FOA-SDK.

The handbook is intended to become the one-stop entry point for:

- setting up a supported modding environment;
- creating a mod from an empty workspace to a reviewed package;
- understanding Mono and IL2CPP as separate runtime routes;
- finding documented game systems, stable identities, relationships, and known extension points;
- locating reviewed hook records and compatibility risks;
- using FOA-SDK authoring, validation, packaging, provider, and adapter systems;
- debugging, support, release, migration, and maintenance work;
- seeing exactly what is known, what is inferred, what is blocked, and what still requires research.

## Ownership boundary

This handbook is a navigation and explanation layer. It does not replace the owners of machine contracts or operational authority:

- `docs/tainted-grail-sdk/` owns FOA-SDK product architecture and implemented-tool documentation;
- `Research/` owns preserved research inputs, exact source and claim registers, and unresolved investigations;
- `Gems/TaintedGrailModdingSDK/` owns required schemas, services, validation, and canonical catalog contracts;
- `Plugins/` owns optional authoring, integration, and runtime-adapter packages;
- `Installer/` owns reviewed suite/package selection and distribution contracts.

A handbook page may explain those systems and link to them. It must not duplicate a schema, invent a native identifier, promote evidence, or grant build, deployment, runtime, save, signing, publication, catalog-mutation, or evidence-promotion authority.

## Source baseline

The initial migration is pinned to:

```text
repository: theb0yys/Tainted-Grail-The-Fall-of-Avalon-mods
branch: main
commit: d7e740e7f167b73152b53409e483dab07d80d048
captured: 2026-07-21
licence: NOASSERTION at repository root
```

Because the source repository has no root licence declaration at that commit, the migration begins by extracting structure, claims, provenance, and project-owned knowledge. It does not bulk-copy source files, binaries, game content, decompiled code, upstream assets, or generated live outputs.

## Planned structure

```text
docs/tainted-grail-modding/
├── README.md                    this hub and learning paths
├── INFORMATION_ARCHITECTURE.md  ownership, page types, metadata, and navigation rules
├── PORTING_PLAN.md              ordered migration and acceptance process
├── SOURCE_MAP.md                exact upstream-to-SDK content map and status
├── RESEARCH_BACKLOG.md          unresolved questions and completion gates
├── getting-started/             environment, first mod, build, test, package
├── process/                     research, design, implementation, review, validation, release
├── systems/                     game-system and authoring-system reference
├── hooks/                       exact extension-point and patch-target catalogue
├── runtime/                     Mono, IL2CPP, loaders, adapters, deployment boundaries
└── reference/                   glossary, identities, versions, compatibility, troubleshooting indexes
```

Each folder starts as an index. Detailed pages are added only when their source, version scope, evidence state, ownership, and limitations are explicit.

## Learning paths

### Make a first mod

1. [Getting started](getting-started/README.md)
2. [Mod-development process](process/README.md)
3. [Runtime routes](runtime/README.md)
4. [Hook catalogue](hooks/README.md)
5. [Reference index](reference/README.md)

### Research or extend a game system

1. [System reference](systems/README.md)
2. `Research/tainted-grail-system-ports/`
3. the canonical catalog and evidence guides under `docs/tainted-grail-sdk/`
4. the relevant hook records and version profile
5. the governed candidate-evidence and review process

### Port knowledge from the source repository

1. [Information architecture](INFORMATION_ARCHITECTURE.md)
2. [Source map](SOURCE_MAP.md)
3. [Porting plan](PORTING_PLAN.md)
4. [Research backlog](RESEARCH_BACKLOG.md)

## Non-negotiable rules

- Version, game branch, runtime, loader, and evidence scope are part of every technical claim.
- Mono and IL2CPP observations are never silently combined.
- A prose page is not runtime authority.
- A candidate hook is not a supported hook until its exact target and profile are verified.
- Decompiled game source is not copied wholesale; only concise target facts and permitted evidence are recorded.
- Game assemblies, BepInEx binaries, compiled mod DLLs, saves, private paths, credentials, logs, and unreviewed commercial assets are not committed.
- Existing canonical schemas, identifiers, source registers, and relationship records are linked rather than rewritten.
- Uncertainty and negative results remain visible.

## Current state

The root design, migration model, source map, hook-record contract, and initial research backlog are established here. The existing selective system ports remain authoritative for Tainted Framework, Tainted Interface, provider-neutral intake, Road Atlas, Avalon AI, and inert Mono/IL2CPP route contracts. The broader handbook content is not yet migrated.