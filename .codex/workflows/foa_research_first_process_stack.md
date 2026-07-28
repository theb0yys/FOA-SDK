# FOA-SDK Research-First Process Stack

## Stack Order

1. `foa-sdk-research-sentinel`
2. `foa-sdk-research-authority`
3. `foa-change-impact-classifier`
4. Narrow domain skill selected for the owner surface
5. `foa-contract-persistence-compatibility-gates` when applicable
6. `foa-test-gap-enforcer` for implementation changes
7. `foa-performance-budget-gates` for performance-relevant changes
8. `foa-evidence-pack-auditor`
9. `foa-pr-release-captain` for GitHub or release handoff

Steps 1 through 4 are mandatory before editing. Later applicable steps may not be silently omitted. An incomplete applicable gate makes validation partial or blocked.

## Required Path

```text
request
-> controlling documents
-> domain research and gates
-> owner surface
-> compatibility/test/performance gates
-> artifact or deployment-review gate
-> evidence pack
-> GitHub handoff
-> next researched stop/process
```

## Hard Stop

Stop when authority, owner, protected-file status, compatibility, required proof, or the next researched action cannot be established. Produce a Deep Research Brief rather than guessing.

## Runtime Proof Status

Static review, source inspection, configure success, compilation, unit tests, interchange validation, package preview, or adapter build do not constitute Fall of Avalon runtime proof. Runtime sign-off requires explicit, lawful, packaged evidence from the exact target installation and adapter path. Otherwise state `runtime sign-off not performed`.
