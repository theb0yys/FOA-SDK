# Gate 5 Canonical Interchange Implementation Authority Decision

Status: pending maintainer decision; implementation not authorized

Design baseline: `eb840862c9d3e239dec91770495c7669c00d10df` (`main`), observed 22 July 2026

Current-main reconciliation: `3fb95284f7cbc259a3c4ab0ba4469be0c9d7baaf`

Proposed design:
`../../../docs/tainted-grail-sdk/CANONICAL_INTERCHANGE_GATE_5_DESIGN.md`

Normative design addendum:
`../../../docs/tainted-grail-sdk/CANONICAL_INTERCHANGE_GATE_5_DESIGN_DECISIONS.md`

Current-main reconciliation record:
`../../../docs/tainted-grail-sdk/CANONICAL_INTERCHANGE_GATE_5_CURRENT_MAIN_RECONCILIATION_2026-07-22.md`

## Decision state

```text
phase_9_entry_accepted: false
gate_5_design_accepted: false
gate_5_implementation_authorized: false
gate_6_authorized: false
```

This file is the only research-track record that may later state that the first Gate 5 implementation slice is
authorized. Research acceptance, design completion, design-PR merge, CI success, or a compiled prototype does not
change these values automatically.

## Proposed authorized unit

When accepted, Gate 5 would authorize only:

```text
Core typed Schema-1 value contracts
+ dedicated canonical JSON parsing and serialization
+ declared and semantic fingerprints
+ pure intrinsic validation and deterministic issues
+ pure adjacent migration dispatch
+ public structural schema documentation and examples
+ synthetic Core-only compiled fixtures
+ repository source-tree consistency validation
```

The exact source, test, schema, validator, documentation, and workflow paths are listed in the proposed design.
Any additional production file, dependency, service, registry, persistence path, provider, host integration, or
runtime surface requires a new decision.

## Prerequisites for acceptance

The maintainer may set the decision to accepted only after confirming:

- [ ] the canonical interchange research conclusion is merged;
- [ ] repository drift is reconciled to the implementation base;
- [ ] the Phase 9 entry prerequisite is explicitly accepted for this Gate 5 unit;
- [ ] the proposed design, normative addendum, and current-main reconciliation are reviewed and accepted;
- [ ] the exact implementation changed-file list is approved;
- [ ] the all-false authority matrix is accepted;
- [ ] hosted static golden-fixture checks and exact-head compiled canonical-byte evidence in the approved O3DE
      build environment are accepted;
- [ ] exact-head O3DE configure/build and the dedicated compiled test target are mandatory;
- [ ] public schema/example consistency validation is mandatory;
- [ ] Gate 6 and every later gate remain closed.

## Capabilities that remain prohibited

Acceptance of this Gate 5 unit would not authorize:

- Framework or Editor services consuming the contracts;
- mutable registries, persistence, workspace access, or catalogue changes;
- filesystem package loading, hashing, staging, or publication;
- environment, registry, clock, credential, or network access;
- process launch, supervision, cancellation, or cleanup;
- provider discovery or execution;
- Blender discovery, add-on installation, import, export, or execution;
- Unity discovery, project creation/mutation, package installation, import, build, or execution;
- O3DE Asset Processor source-root publication;
- proprietary FoA project or game-install inspection;
- runtime payload mapping or runtime-adapter build/call;
- deployment, backup, restore, rollback execution, or game launch;
- save access or mutation;
- evidence registration or promotion;
- archive assembly, signing, verification, upload, or publication;
- compatibility certification or release approval.

## Required implementation evidence

An authorized implementation PR must provide:

1. exact implementation base and changed-file inventory;
2. DCO-signed commits;
3. hosted non-compiled validation for the exact PR head;
4. exact-head O3DE configure and Core compilation in the approved build environment;
5. dedicated canonical-interchange compiled test results from that exact head;
6. public golden canonical-byte fixtures verified by hosted static checks and reproduced by the compiled C++ tests;
7. synthetic positive and negative fixture results;
8. caller-input non-mutation proof;
9. public schema/example consistency results;
10. explicit statement that no Blender, Unity, Asset Processor, runtime, deployment, save, signing, publication, or
    compatibility operation was performed.

A hosted Linux O3DE C++ build is not required unless an approved hosted Linux O3DE environment exists for the
implementation head. Static Python fixture comparison must not be presented as compiled C++ evidence.

## Acceptance record

The acceptance change must replace the pending state with all of:

```text
phase_9_entry_accepted: true
gate_5_design_accepted: true
gate_5_implementation_authorized: true
gate_6_authorized: false
accepted_repository_base: <full 40-character commit>
accepted_design_commit: <full 40-character commit>
accepted_by: <maintainer identity>
accepted_at_utc: <caller-supplied UTC timestamp>
```

The acceptance commit message must name Gate 5 and state that it is contract-only. The acceptance record must not
be inferred from a conversation, issue label, CI status, merge state, or implementation commit.

## Revocation and drift

Authority is invalidated before implementation starts when:

- `main` moves and the drift changes Core ownership, build dependencies, canonical helpers, validation policy,
  public schema policy, or gate order;
- the implementation changed-file scope exceeds the accepted list;
- a new dependency or operational capability is proposed;
- the design is materially amended without a renewed decision.

Non-material drift may be reconciled in the implementation PR with exact comparison evidence and maintainer
confirmation.

## Current decision

Gate 5 implementation remains blocked. The next action is explicit maintainer review of the proposed design and
this authority record. No source implementation may begin under this pending state.

## Permanent non-authority statement

This pending decision creates no implementation, service, persistence, filesystem, process, provider, native-host,
runtime, deployment, save, evidence-promotion, signing, archive, upload, publication, compatibility, or release
authority.