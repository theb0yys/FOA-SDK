# Authoring Interchange and Assets

## Scope

This area covers project-owned or synthetic authoring content exchanged among Blender, O3DE, and a synthetic
or user-owned Unity Editor test project. It does not cover a proprietary FoA project or a running game.

## Contract topics

- one versioned `manifest.tginterchange.json` package boundary;
- stable `PackageId`, `AssetId`, `RevisionFingerprint`, `BindingId`, and `MappingId` identities;
- exact native bindings without treating display names or paths as identity;
- contained relative payload paths, byte sizes, media types, and SHA-256 observations;
- explicit coordinate basis, units, transform order, pivot, timebase, and conversion matrices;
- bounded mesh, texture, material, skeleton, animation, collider, and metadata declarations;
- provenance, authorship, licence, attribution, modification, and redistribution state;
- ordered transformations, typed losses, external qualification evidence, and exact toolchain locks;
- reverse and round-trip behavior with declared numeric tolerances.

Gate 0 owns inert Core-only outer handoff/result envelopes. No service consumes them and every authority and
performed flag remains false. A later process operation may bind the interchange package through those
envelopes, but the envelopes do not replace its identity, transformation, provenance, and loss contracts.

## Accepted research conclusion

[`CANONICAL_INTERCHANGE_SCHEMA_AND_IDENTITY_RESEARCH.md`](CANONICAL_INTERCHANGE_SCHEMA_AND_IDENTITY_RESEARCH.md)
answers the focused Schema-1 structure and identity question.

The conclusion was written against `504e10b27e46fceae4d68af200118edca27b4d1b` and reconciled through
`3d50df1ea3aaf97a5148ae4ebb5c5ade8336e6d2` by
[`CURRENT_MAIN_RECONCILIATION_2026-07-22.md`](CURRENT_MAIN_RECONCILIATION_2026-07-22.md). The Gate 5 design is
based on later `main` commit `eb840862c9d3e239dec91770495c7669c00d10df`.

The accepted research direction is:

```text
one authoritative manifest
+ typed engine-neutral domain documents
+ contained ordered payload inventory
+ exact native bindings
+ first-class identity mapping events
+ bounded extension namespaces
+ external Framework-owned qualification evidence
```

Schema 1 is directory-only. It uses a dedicated `foa-interchange-canonical-json-v1` profile, ASCII semantic
tokens, engine-neutral identities, explicit migration, deterministic blocker codes, and the fixed canonical basis
`+X` right, `+Y` forward, `+Z` up, right-handed, metres, radians, and seconds. Matrices use column vectors,
matrix-on-left multiplication, and column-major storage.

The accepted conclusion remains research-only and grants no implementation or operational authority.

## Proposed Gate 5 contract design

[`GATE_5_CONTRACT_IMPLEMENTATION_DESIGN_QUESTION.md`](GATE_5_CONTRACT_IMPLEMENTATION_DESIGN_QUESTION.md) is
answered by:

- [`../../../../../docs/tainted-grail-sdk/CANONICAL_INTERCHANGE_GATE_5_DESIGN.md`](../../../../../docs/tainted-grail-sdk/CANONICAL_INTERCHANGE_GATE_5_DESIGN.md);
- [`../../../../../docs/tainted-grail-sdk/CANONICAL_INTERCHANGE_GATE_5_DESIGN_DECISIONS.md`](../../../../../docs/tainted-grail-sdk/CANONICAL_INTERCHANGE_GATE_5_DESIGN_DECISIONS.md).

The proposed design fixes:

```text
Core type and file ownership
→ dedicated canonical parser/writer
→ intrinsic validator and exact issue ordering
→ pure migration dispatch
→ public schema package
→ Core-only compiled fixtures
→ source-tree consistency validation
→ explicit all-false authority matrix
```

The design is ready for maintainer review but is not implementation authority.

## Resolved design points

- Four exact Core translation-unit pairs own types, canonical bytes, validation, and migration.
- A dedicated test target links Core.Static and AzTest only.
- Core gains no Framework, Editor, Qt, filesystem, provider, host, or runtime dependency.
- Exact count, byte, path, token, digest, and nesting limits are selected.
- JSON tokens retain the research-approved hyphenated spellings.
- UTF-8 presentation strings are preserved without NFC normalization.
- Optional `display_name` is omitted when empty rather than normalized to an empty property.
- The intrinsic and reserved cross-layer issue-code inventories are complete.
- Schema-1 migration supports exact `1 -> 1` identity and fails closed for every unavailable route.
- The exact public schema, test, validator, and proposed changed-file layout is selected.
- Every operational capability remains absent or false.

## Remaining gate decision

Implementation remains blocked until:

1. the Phase 9 entry prerequisite is explicitly accepted for Gate 5;
2. the proposed design and addendum receive explicit maintainer acceptance;
3. an authority record names the exact implementation base, contract-only scope, tests, evidence, and prohibited
   capabilities.

Exact Blender and Unity versions remain unqualified. Asset Processor candidate publication remains a later Gate 7
design. No cross-host compatibility claim exists without exact fixtures and accepted evidence.

## Authority boundary

Format documentation, accepted research, and an accepted design do not establish cross-engine compatibility or
implementation authority. Gate 6 and later remain closed. No format or round-trip claim becomes supported until
the exact adapters, hosts, profiles, and fixtures pass the applicable implementation gates.