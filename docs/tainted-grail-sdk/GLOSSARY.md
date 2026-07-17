# Glossary

## Adapter

A separate component that performs native FoA, Unity, BepInEx, or Harmony operations after receiving a reviewed and permitted handoff. Adapters own runtime compatibility, persistence, cleanup, and rollback.

## Alias

A non-canonical name or reference associated with one subject. An alias helps search but does not replace stable identity.

## Avalon Core

The evidence, trust, catalog, planning, permission, and handoff framework associated with the TG modding ecosystem. It is not a gameplay executor.

## Blocker

A durable or computed explanation of why a record, workflow, or usage is unavailable. Blockers fail closed and should identify corrective action when known.

## Canonical catalog

The reviewed, queryable store of game knowledge and project-owned records. It preserves identity, evidence, relationships, validation, permissions, conflicts, and versions.

## Claim

A statement proposed from evidence. A claim remains distinct from the evidence that supports it and from a reviewed catalog record.

## Confidence

An assessment of how strongly the available evidence supports a claim or record. Confidence is independent of maturity, validation, risk, and permission.

## Display name

Human-readable label. It is never sufficient as a stable identity or deduplication key.

## Evidence

An attributed observation or statement extracted from one source, with source, fingerprint, profile, version, locator, and subject linkage.

## Exact native reference

The unmodified game-facing identifier, GUID, path, address, or reference used by FoA or its content. Exact native references are preserved byte-for-byte unless a documented format says otherwise.

## FoA

Tainted Grail: The Fall of Avalon, the separate Unity runtime targeted by the modding editor and SDK.

## Game profile

The exact build context associated with a lawful FoA installation: profile ID, game version, branch, Mono/IL2CPP target, Unity/BepInEx versions, assemblies, paths, and content scopes.

## Handoff

A reviewed, non-executing work order or manifest passed from editor/knowledge systems to a separate runtime adapter.

## Identity kind

Classification describing how a subject is identified, such as native, synthetic, composite, alias, source-scoped, or unknown.

## Import issue

Structured warning or error produced while parsing, validating, persisting, or reloading source/evidence documents.

## Importer contract

Versioned declaration of an importer's stable ID, supported source kinds/extensions, extraction behavior, limits, and output rules.

## Maturity

Research or review progression of a claim or record. It does not automatically imply validation or runtime permission.

## Missing reference

A required identity or relationship that has not been resolved with sufficient evidence.

## Native record

A record representing content or behavior that belongs to the game. It preserves the exact native reference and is not owned as custom pack content.

## O3DE

Open 3D Engine, used as the editor and tooling host in this repository.

## Operational risk

Assessment of potential harm from using a record or action, including crashes, save impact, story impact, density, compatibility, or cleanup failure.

## Pack

The stable ownership and release unit for a mod project. It defines identity, owner, version, compatibility, dependencies, save impact, resources, and release intent.

## Permission

Explicit usage-specific authorisation based on evidence, validation, version, and risk. Permission is not inferred from parsing, maturity, confidence, or UI display.

## Prohibition

Explicit denial of a usage, such as no spawn, no mutate, no save write, or no story use.

## Record ID

Stable project-facing identifier for a catalog or governance record.

## Relationship

Typed link between subjects, such as containment, upgrade, faction membership, route step, recipe ingredient, spawn composition, or evidence support.

## Runtime target

The game's scripting/runtime mode relevant to tooling and adapters. Current game-profile values are `Mono` or `IL2CPP`.

## Source

Immutable metadata about an imported artifact, including fingerprint, exact build binding, tool/importer details, locator, limitations, media type, and size.

## Source fingerprint

SHA-256 digest used to identify an imported artifact. It proves content equality for the hashed bytes, not factual truth or permission.

## Source-scoped reference

Identifier meaningful only within a particular source or evidence context. It must not be treated as a global native identity without reconciliation.

## Subject reference

Stable reference to the thing a claim, evidence record, blocker, or relationship concerns.

## Synthetic record

Project-created or custom content identity. It must be owned by a pack and must not borrow a native game identity.

## Validation

Recorded check of a claim, record, relationship, format, or use against defined criteria. Validation is scoped to evidence, version, method, and intended usage.

## Workspace

Local authoring boundary containing exact game profiles, pack manifests, sources, evidence, catalog data, content definitions, build output, staging, reports, and later deployment plans.
