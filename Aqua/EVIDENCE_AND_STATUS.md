# Kandra Evidence and Status Model

Status: derivative reference.

## 1. Evidence lanes must remain separate

Kandra distinguishes evidence by what it can actually prove.

### Research context

Examples:

- returned ChatGPT Deep Research report;
- public documentation review;
- package metadata reconnaissance;
- first-party or third-party technical documentation.

Use: discover and scope claims.

Does not prove: live runtime behavior, implementation correctness, installed-machine state, authorization, or promotion.

### Decompilation/static external evidence

Examples:

- assembly metadata;
- CIL/native static inspection;
- serialized metadata inspection where lawful and authorised;
- static call graphs.

Use: establish static identities, code paths, or bounded format behavior.

Does not prove: that the path executes in the user's live game, that state persists, or that runtime side effects occur.

### Repository/static evidence

Examples:

- exact source path and commit;
- schema/policy inspection;
- deterministic static validators;
- source-register and claim-register reconciliation.

Use: establish current repository contracts and static facts.

Does not prove: compiled behavior, Editor interaction, installed-game behavior, or external operational effects.

### Host/compiled evidence

Examples:

- configure;
- build;
- compiled tests;
- exact-head CTest;
- deterministic host fixtures.

Use: prove the selected compiled host path.

Does not prove: Editor interaction unless the Editor path actually ran, or target-game runtime compatibility.

### Editor/UI evidence

Examples:

- O3DE Editor pane lifecycle;
- visual/manual interaction;
- platform UI automation;
- saved Editor state.

Use: prove interaction in the actual Editor/UI layer.

Does not prove: Fall of Avalon runtime behavior or external deployment unless those operations also ran.

### Operational/runtime evidence

Examples:

- installer lifecycle;
- deployment and rollback;
- target-game launch/verification;
- runtime adapter execution;
- signing/publication/release operations.

Use: prove the exact operation that ran.

One operational lane does not prove another. An installer smoke does not prove game runtime; runtime launch does not prove save compatibility; package verification does not prove publication.

## 2. FOA-SDK validation layers L0-L4

| Layer | Meaning | Typical proof |
| --- | --- | --- |
| L0 | Repository/static | structure, policy, static validation, source checks |
| L1 | Unit/contract | deterministic focused tests, malformed input, migration/round trip |
| L2 | Configure/build/compiled host | pinned O3DE configure/build and compiled tests |
| L3 | Editor/UI/manual host | real Editor or platform interaction |
| L4 | Operational/runtime | installer, deployment, rollback, game runtime, signing, publication, release |

Select layers by changed surface and risk. Do not run unrelated layers merely for ceremony, and do not omit an applicable layer because a cheaper one passed.

## 3. Exact result states

Use only:

- `PASSED` - the stated check actually ran against the stated subject and met its acceptance condition.
- `FAILED` - the stated check ran and did not meet its acceptance condition.
- `PARTIAL` - useful evidence exists but does not cover the full required claim.
- `BLOCKED` - a required prerequisite, permission, tool, evidence lane, or authority is unavailable.
- `NOT_RUN` - applicable check was not executed.
- `NOT_APPLICABLE` - the check cannot affect the changed surface or claim.

Never use "probably passed", "effectively passed", or "good enough" as evidence states.

## 4. Research claim states

The current terrain research demonstrates a useful claim vocabulary:

- `design-context` - approved product direction/proposal; not a source-format fact or implementation permit.
- `repository-observed` - exact repository content at a recorded revision supports the claim.
- `source-supported` - one or more durable sources support the scoped claim.
- `static-report-supported` - a preserved static/decompilation report supports the claim; underlying analysis may not have been independently reproduced.
- `input-observed` - a preserved report records the observation but durable reconciliation may be incomplete.
- `inference` - bounded conclusion derived from identified evidence.
- `unknown` - required proof is missing.
- `contradicted` - accepted evidence conflicts with the claim.
- `superseded` - a later reviewed record replaces the claim.

A claim state does not grant implementation, runtime, packaging, deployment, publication, extraction, or promotion authority.

## 5. Source quality requirements for consequential claims

A decision-influencing claim should record:

- stable claim ID;
- direct source locator;
- source publisher/origin;
- version, package version, repository revision, or documentation revision where relevant;
- retrieval/observation date;
- exact scoped statement of support;
- limitation: what the source does not establish;
- evidence lane;
- contradiction/supersession state.

A search-result snippet, opaque chat citation token, mutable display name, unattributed excerpt, model memory, or passing unrelated test is not durable claim evidence.

## 6. Binding rules

Evidence must be bound to the thing it claims to prove.

Examples:

- repository observation -> exact path + full commit;
- build result -> exact source commit + build configuration + target;
- test result -> exact target/pattern + source commit + nonzero test set;
- runtime result -> exact installation/build/profile + operation + observed output;
- preserved external report -> exact file/fingerprint + origin + intake date;
- binary/static report -> binary identity/hash when available + method + limitation.

## 7. Forbidden substitutions

Do not substitute:

- public docs for binary/static identity;
- static identity for live call execution;
- build for Editor UI;
- Editor UI for game runtime;
- game launch for persistence/save correctness;
- checksum for reviewer authorization;
- research report for implementation permission;
- proposed experiment for executed experiment;
- generated output for canonical source;
- source defaults for shipped serialized values;
- nearby or derived metadata for missing authoritative fields.

When the required lane is unavailable, keep the claim `UNKNOWN`, `PARTIAL`, `BLOCKED`, or `NOT_RUN`.
