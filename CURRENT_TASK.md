# Current Task

This file records the active FOA-SDK task state so work can continue without treating conversation history as authority.

## Goal

- Record the repository-owner-authorised M0 governance and implementation-authority decision for the capability-execution program.
- Authorise exactly one later implementation batch: M1 additive Core contracts.
- Name the exact M1 owner paths, contract boundary, threat/failure analysis, migration policy, stop conditions, and required proof before product-source work begins.

## Files Currently Involved

- `CURRENT_TASK.md`
- `DECISIONS.md`
- `docs/tainted-grail-sdk/CAPABILITY_EXECUTION_M0_IMPLEMENTATION_AUTHORITY.md`

## Constraints

- Documentation and decision state only in this task; no product implementation source changes.
- The M0 decision becomes effective only after maintainer audit and merge to `main`.
- M1 is Core-only and additive. Existing inert V1 contracts, canonical bytes, validators, result meanings, and false authority flags remain unchanged.
- M2 process supervision, M3 Framework orchestration, M4 planner adaptation, M5 synthetic execution, M6 heightmap runtime work, and every later batch remain unauthorised.
- No process launch, filesystem mutation, persistence, UI, provider invocation, package assembly, deployment, game launch, runtime mutation, save write, signing, publication, catalog mutation, or evidence promotion is authorised by this documentation task.
- Work remains on `architecture/capability-execution-m0-authority` and enters `main` only through a maintainer-audited pull request.

## Completed Work

- Pull request #234 merged the canonical Capability Execution Contract and shared Build -> Package -> Deploy -> Launch -> Verify architecture into `main` at `f5d9883e24aca6b8910600fd92a809cc3aa07253`.
- The exact M1 contract scope, owner paths, build/test ownership, threat and failure controls, compatibility policy, migration policy, required proof, acceptance mapping, and stop conditions were defined in the M0 authority record.
- The M0 record explicitly excludes implementation and every batch after M1.

## Remaining Work

- Run documentation/process validation available for the final M0 branch head.
- Complete maintainer audit and merge of the focused M0 decision pull request.
- Only after M0 merges, create `implementation/capability-execution-m1-core-contracts` from the accepted `main` head and implement only the authorised paths.

## Do Not Change

- Product implementation source in the M0 documentation pull request.
- Any existing V1 contract or validator.
- Framework, Editor, ExternalToolchain, provider, plug-in, installer, runtime-adapter, game, save, signing, publication, or deployment behaviour.
- Protected files, external build output, Unity projects, or Fall of Avalon installations.
- Unrelated repository files.

## Next Concrete Step

- Maintainer review of the M0 implementation-authority pull request. M1 source work remains blocked until that pull request is merged.
