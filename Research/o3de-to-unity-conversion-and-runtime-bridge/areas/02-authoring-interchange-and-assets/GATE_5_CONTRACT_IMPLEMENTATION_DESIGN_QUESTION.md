# Gate 5 Canonical Interchange Contract Implementation Design Question

Status: answered by proposed focused design; pending explicit maintainer acceptance; no implementation authority

Repository baseline: `eb840862c9d3e239dec91770495c7669c00d10df` (`main`), observed 22 July 2026

Parent research conclusion: `CANONICAL_INTERCHANGE_SCHEMA_AND_IDENTITY_RESEARCH.md`

Proposed design:
`../../../../../docs/tainted-grail-sdk/CANONICAL_INTERCHANGE_GATE_5_DESIGN.md`

Normative design addendum:
`../../../../../docs/tainted-grail-sdk/CANONICAL_INTERCHANGE_GATE_5_DESIGN_DECISIONS.md`

## Design question

What exact Gate 5 Core contract API, source-file ownership, canonical serializer boundary, validator limits,
public schema package, migration interface, and synthetic fixture acceptance matrix should FOA-SDK authorize for
the first contract-only implementation slice while preserving zero filesystem, provider, process, native-host,
runtime, deployment, save, signing, publication, or evidence-promotion authority?

## Answer summary

The proposed first implementation unit is one Core-only contract package containing:

```text
CanonicalInterchangeTypes
→ CanonicalInterchangeCanonical
→ CanonicalInterchangeValidation
→ CanonicalInterchangeMigration
→ public Schema-1 structural documents
→ dedicated Core-only compiled tests
→ source-tree consistency validation
```

The design fixes:

- namespace and exact C++ type inventory;
- four Core source/header pairs and a separate Core-only test target;
- exact bounded counts, byte limits, token grammar, path grammar, and digest representation;
- public `PackageIdV1`, `AssetIdV1`, `RevisionFingerprintV1`, `BindingIdV1`, and `MappingIdV1` wrappers;
- manifest, document, asset, mapping, binding, payload, transformation, loss, provenance, licensing, lock,
  extension, and evidence-reference records;
- no reflection, EBus, interface, registry, service, Framework, Editor, Qt, provider, host, or runtime dependency;
- dedicated canonical parsing/writing over supplied bytes and values only;
- exact property order, stable-key sorting, hyphenated JSON enum tokens, UTF-8 presentation behavior, finite-number
  rules, negative-zero normalization, and fingerprint projections;
- complete intrinsic and reserved cross-layer issue-code inventories with deterministic precedence;
- a pure migration API whose Schema-1 implementation supports exact `1 -> 1` identity and fails closed for every
  unavailable, future, or downgrade route;
- the public schema package and synthetic fixture layout;
- the exact proposed changed-file list and rollback consequences;
- an explicit all-false operational authority matrix.

## Entry prerequisite

The research conclusion is merged. Repository drift is reconciled to the proposed design base.

Implementation may begin only after:

1. the Phase 9 entry prerequisite is explicitly accepted under the controlling roadmap and gate map;
2. this proposed focused design and its addendum receive explicit maintainer acceptance;
3. an authority record names the exact implementation base and contract-only scope.

Research or design acceptance alone does not open Gate 6 or any later gate.

## Deliverable disposition

| Required deliverable | Disposition |
| --- | --- |
| implementation design with ownership/dependency graph | complete in parent design |
| complete C++ contract inventory | complete in parent design |
| canonical serialization and golden-byte decision | complete in parent design and addendum |
| validator/blocker precedence | complete in parent design and addendum |
| migration API and state transitions | complete in parent design |
| public schema package layout | complete in parent design |
| compiled-test and validator plan | complete in parent design |
| exact changed-file list | complete in parent design |
| all-false authority matrix | complete in parent design |
| rollback/compatibility consequences | complete in parent design |
| implementation-PR acceptance checklist | complete in parent design |
| following focused unit | complete in parent design |

## Design acceptance criteria

The design is ready for maintainer decision because:

- every proposed type and file has one owner;
- Core remains independent from Framework, Editor, Qt, filesystem, providers, Blender, Unity, runtime adapters,
  and the target game;
- no second identity, evidence, permission, or catalogue system is introduced;
- canonical bytes, optional-property behavior, tokens, sorting, and fingerprints are explicit;
- every intrinsic invalid class maps to one stable deterministic code;
- Framework-owned codes are reserved but not emitted by Core;
- migration fails closed and never mutates source input;
- fixtures cover accepted research invariants and negative paths;
- the changed-file scope is narrow and contract-only;
- no service consumes the contracts;
- operational and performed capabilities are absent or permanently false;
- Phase 9/Gate 5 authority remains a separate explicit decision.

## Out of scope

- implementation before maintainer acceptance;
- filesystem package loading or publication;
- Asset Processor integration;
- provider discovery or execution;
- Blender add-on or command work;
- Unity package, project, importer, or command work;
- native-host compatibility claims;
- runtime payload mapping;
- deployment, game launch, mutation, save access, signing, or publication.

## Permanent non-authority statement

This answered design question and its proposed design documents do not authorize implementation. They create no
schema service, persistence, filesystem access, process, provider, native-host operation, runtime capability,
deployment, save access, signing, publication, or evidence-promotion authority.