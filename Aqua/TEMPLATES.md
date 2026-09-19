# Kandra Templates

Status: derivative restartability aid.

These templates do not replace repository-owned templates such as `.codex/checklists/deep_research_brief_template.md`. Use the owning template when one exists.

## 1. Kandra research brief

```markdown
# <ID> Research Brief

Status: OPEN

## Request
Owner request:
Blocked decision:
Why the decision is blocked:
Consuming owner/gate:

## Known repository facts
- Controlling source:
- Exact revision:
- Proven:
- Not proven:

## Research questions
1.
2.

## Required method
- Research method:
- Why this method:
- No-substitute constraint:

## Evidence lanes
- Repository/static:
- Public research:
- Decompilation/static:
- Host execution:
- Live runtime:
- Persistence/save:

## Protected boundary
- Data/files to avoid:
- Permission needed:

## Required returned output
- Sources:
- Direct answer per question:
- Contradictions:
- Missing proof:
- Required next evidence lane:
- Implementation authority: none / partial / supported by separate normative source only
```

## 2. Preserved input record

```markdown
# <ID> Preserved Input Record

Original file:
Origin:
Intake date:
Fingerprint:
Version/revision context:
Evidence lane:
Citation limitations:
Protected-data note:

Preservation rule:
The original is retained unchanged. Corrections belong in a separate derivative.
```

## 3. Clean intake

```markdown
# <ID> Clean Intake

Preserved input:
Status: PARTIAL / BLOCKED / ...

Implementation authority created: none
Runtime validation: NOT_RUN
Independent reproduction: NOT_RUN / PASSED / ...

## Accepted scoped conclusions
-

## Rejected/contradicted conclusions
-

## Unknowns
-

## Source bindings
- SRC-...

## Claim bindings
- CLM-...

## Remaining evidence boundary
-
```

## 4. Source register row

```markdown
| Source ID | Source | Exact locator | Publisher/origin | Version/revision/date | Evidence lane | Supports | Does not support |
| --- | --- | --- | --- | --- | --- | --- | --- |
| SRC-001 | | | | | | | |
```

## 5. Claim register row

```markdown
| Claim ID | Claim | State | Sources | Consequence / limitation | Missing proof |
| --- | --- | --- | --- | --- | --- |
| CLM-001 | | unknown | SRC-001 | | |
```

## 6. RH review record

```markdown
# <ID> Kandra Review Record

## RH0 - intake
State:
Findings:
Blockers:

## RH1 - claim evaluation
State:
Claims reviewed:
Source gaps:
Contradictions:

## RH2 - domain/repository review
State:
Owning system:
Normative sources:
Implementation authority:
Required downstream gates:

## RH3 - independent review
Reviewer:
State:
Independent findings:
Disagreements:

## RH4 - validation/evidence review
State:
Required L0:
Required L1:
Required L2:
Required L3:
Required L4:
Unavailable evidence:

## RH5 - human promotion
Promotion owner:
Decision: UNPROMOTED / ACCEPTED / REJECTED / RETURNED
Normative destination:
Notes:
```

## 7. Promotion candidate

```markdown
# <ID> Promotion Candidate

Research inputs:
Claim set:
Adverse evidence:
Remaining unknowns:
Owning subsystem:
Proposed normative destination:
Schema/version impact:
Migration/rejection impact:
Security/legal impact:
Compatibility impact:
Validation impact:
Rollback/recovery impact:

This document is a promotion candidate only. It is not an accepted decision until the human promotion owner records acceptance in the normative destination.
```

## 8. Implementation handoff

```markdown
# <ID> Implementation Handoff

Owner request:
Normative authority:
Scope:
Out of scope:
Branch:
Changed files:

Validation:
- L0:
- L1:
- L2:
- L3:
- L4:

Research context used:
Protected-data impact:
Remaining uncertainty:
Commit:
Pull request:
Merge: NOT_RUN unless explicitly performed and verified
Release/deployment/signing/publication: NOT_RUN unless explicitly performed and verified
```

## 9. Blocker record

```markdown
State: BLOCKED

Operation:
Verified blocker:
Required missing authority/tool/evidence/permission:
Substitute work performed: none
Repository state changed: no, unless separately listed
Next action: only if explicitly authorised
```
