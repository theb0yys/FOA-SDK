# Canonical Interchange Research — Current Main Reconciliation

Status: research reconciliation; no implementation authority

Previous accepted-conclusion baseline: `504e10b27e46fceae4d68af200118edca27b4d1b`

Current repository baseline: `3d50df1ea3aaf97a5148ae4ebb5c5ade8336e6d2` (`main`), observed 22 July 2026

Accepted conclusion: `CANONICAL_INTERCHANGE_SCHEMA_AND_IDENTITY_RESEARCH.md`

## Exact comparison

https://github.com/theb0yys/FOA-SDK/compare/504e10b27e46fceae4d68af200118edca27b4d1b...3d50df1ea3aaf97a5148ae4ebb5c5ade8336e6d2

The range contains 26 commits. Its material changes are:

- a governed cross-module `ExtensionRequestBus` over the existing `ExtensionAPI`;
- hardened Framework path-policy and extension-request ownership;
- independently selectable Road Atlas and Avalon AI authoring Tool Gem vertical slices;
- plug-in manifest, lifecycle, repository, and UI validation hardening;
- public semantic-hook and modding-handbook additions;
- documentation and fixture updates associated with those units.

## Effect on the accepted schema research

The range strengthens, rather than contradicts, these accepted conclusions:

- optional authoring systems belong beneath `Plugins/Authoring`;
- plug-ins use the governed ExtensionAPI boundary and receive no mutable workspace, catalog, runtime, deployment,
  save, signing, publication, or evidence-promotion authority;
- Road Atlas and Avalon AI remain typed domain owners whose documents can be bound through the engine-neutral
  interchange package;
- Framework retains path-policy, workspace, persistence, and candidate-before-publish ownership;
- optional authoring Tool Gems do not become native-host or runtime authority.

The range does not change:

- the one-manifest, typed-document, contained-payload architecture;
- canonical identity and native-binding separation;
- the accepted canonical JSON profile;
- the spatial, vector, matrix, unit, and time conventions;
- the transformation, loss, provenance, legal, error, migration, and fixture decisions;
- Blender-first qualification;
- O3DE round-trip ordering;
- Unity Editor-only interchange ordering;
- the separate runtime-adapter lane.

## Gate and authority reconciliation

The controlling implementation-gate mapping is unchanged. It continues to state that:

- Gate 5 owns Schema-1 contracts, canonical identity, transformations, losses, locks, package validation, and
  synthetic fixtures;
- every focused durable-schema unit requires its own design review;
- Gates 1 and later begin only after the ordered first-party capability prerequisite stabilizes and the Phase 9
  entry gate is accepted;
- research approval does not satisfy Phase 9 entry or any implementation gate.

The Road Atlas and Avalon AI authoring Tool Gems demonstrate active Phase 9 extension work, but the repository
contains no explicit acceptance statement opening Gate 5 implementation. Gate 5 therefore remains blocked
pending a separate Phase 9 entry and focused implementation-design decision.

## Reconciliation conclusion

`CANONICAL_INTERCHANGE_SCHEMA_AND_IDENTITY_RESEARCH.md` remains accepted as the Area 02 research conclusion when
read with this exact-current-main addendum. No schema implementation, serializer, validator, migration service,
filesystem operation, provider, process, Blender or Unity execution, native-host claim, runtime behavior,
deployment, save access, evidence promotion, signing, or publication is authorized.
