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

The conclusion was written against `504e10b27e46fceae4d68af200118edca27b4d1b` and is reconciled through current
`main` at `3d50df1ea3aaf97a5148ae4ebb5c5ade8336e6d2` by
[`CURRENT_MAIN_RECONCILIATION_2026-07-22.md`](CURRENT_MAIN_RECONCILIATION_2026-07-22.md).

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

## Resolved reconciliation points

- The canonical forward axis, vector convention, matrix storage, multiplication, and mapping direction are now
  explicit.
- FBX remains one payload candidate rather than the schema or a presumed deterministic container.
- External source-native references remain non-material provenance or become contained fingerprinted payloads.
- Schema evolution and canonical errors fail closed.
- Merge and split use first-class mapping records rather than parent references alone.
- Qualification evidence remains Framework-owned and external to semantic package content.
- Schema 1 uses a contained directory; deterministic archive assembly remains later packaging work.
- The current ExtensionRequestBus and Road Atlas/Avalon AI Tool Gems confirm optional authoring ownership without
  opening Gate 5 or changing the interchange decisions.

## Remaining qualification points

- Gate 5 implementation API and file ownership are not authorized or designed yet.
- Exact maximum sizes, counts, and public JSON Schema packaging remain open for Gate 5 design.
- Exact Blender and Unity versions remain unqualified.
- Asset Processor candidate publication remains a later Gate 7 design and fixture concern.
- No cross-host compatibility claim exists without exact fixtures and accepted evidence.

## Next focused question

The research process advances to
[`GATE_5_CONTRACT_IMPLEMENTATION_DESIGN_QUESTION.md`](GATE_5_CONTRACT_IMPLEMENTATION_DESIGN_QUESTION.md).

That question asks for the exact Core API, canonical serializer boundary, validator limits, migration interface,
public schema package, synthetic fixture matrix, source ownership, and no-authority acceptance boundary for the
first Gate 5 contract-only implementation slice.

## Authority boundary

Format documentation, accepted research, and a successful import do not establish cross-engine compatibility or
implementation authority. Phase 9 entry and a separately accepted Gate 5 design remain prerequisites. No format
or round-trip claim becomes supported until the exact adapters, hosts, profiles, and fixtures pass the applicable
implementation gates.
