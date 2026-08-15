# FOA-SDK Research-First Process Stack

## Stack Order

1. `foa-sdk-research-sentinel`
2. `foa-sdk-research-authority`
3. `foa-change-impact-classifier`
4. Narrow domain skill selected for the owner surface
5. `.codex/workflows/foa_capability_execution_contract.md` when capability, adapter, provider execution, build-manifest, package, deployment, launch, verification, rollback, execution-result, release-assembly/signing, or artifact-ownership work is involved
6. `foa-contract-persistence-compatibility-gates` when applicable
7. `foa-test-gap-enforcer` for implementation changes
8. `foa-performance-budget-gates` for performance-relevant changes
9. `foa-evidence-pack-auditor`
10. `foa-pr-release-captain` for GitHub or release handoff

Steps 1 through 4 are mandatory before editing. Step 5 is mandatory for its trigger surfaces. Later applicable steps may not be silently omitted. An incomplete applicable gate makes validation partial or blocked.

## Required Path

```text
request
-> controlling documents
-> domain research and gates
-> owner surface
-> capability-execution contract when applicable
-> compatibility/test/performance gates
-> artifact or deployment-review gate
-> evidence pack
-> GitHub handoff
-> next researched stop/process
```

## Capability Execution Lock

The shared production path is:

```text
domain materialisation when required
-> Build
-> Package
-> Deploy
-> Launch
-> Verify
-> assessment
-> reconciliation
-> human promotion when required
```

Existing inert V1 contracts stay inert. Preview and execute remain separate. Provider resolution, policy, authorisation, artifact ownership, idempotency, rollback, receipts, evidence projection, assessment, and promotion may not be collapsed.

## Hard Stop

Stop when authority, owner, protected-file status, compatibility, required proof, provider binding, artifact ownership, rollback plan, or the next researched action cannot be established. Produce a Deep Research Brief rather than guessing.

## Runtime Proof Status

Static review, source inspection, configure success, compilation, unit tests, interchange validation, package preview, or adapter build do not constitute Fall of Avalon runtime proof. Runtime sign-off requires explicit, lawful, packaged evidence from the exact target installation and adapter path. Otherwise state `runtime sign-off not performed`.
