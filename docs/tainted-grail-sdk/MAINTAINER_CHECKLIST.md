# Maintainer Checklist

## Per issue

- confirm the issue uses the correct form;
- remove secrets, private paths, proprietary data, or unsafe payloads;
- label scope, risk, domain, and status;
- identify duplicate or upstream issues;
- decide whether design review is required;
- state acceptance criteria and required evidence;
- route security and conduct matters to private handling.

## Before approving implementation

- architecture owner identified;
- editor/runtime boundary preserved;
- identity and pack ownership defined;
- source/evidence requirements defined;
- persistence and schema version defined;
- compatibility, save, deployment, and rollback impact considered;
- security and privacy risks considered;
- test plan is proportionate to risk;
- no redistribution problem is apparent.

## Per pull request

### Scope and governance

- PR targets `main` from `foa-development`;
- template is complete;
- issue/design is linked when required;
- commits include DCO sign-off;
- change is focused and reviewable;
- conflicts of interest disclosed.

### Architecture and safety

- no FoA runtime mutation entered editor/knowledge code;
- exact references and identities remain correct;
- synthetic records are pack-owned;
- evidence, records, validation, and permission remain separate;
- missing proof fails closed;
- untrusted input is bounded;
- file writes stay in approved roots;
- schema and migrations are explicit.

### Code quality

- explicit includes and dependencies;
- ownership and lifetimes clear;
- errors actionable;
- UI delegates domain logic;
- persistence is service-owned;
- no debug code, secrets, or local paths;
- tests cover success and failure;
- documentation and changelog updated.

### Checks and review

- focused TG SDK validator passed;
- repository validation passed;
- applicable O3DE host builds passed;
- required tests passed;
- all review threads resolved;
- maintainer approval recorded;
- PR is ready, not draft;
- pending checks are not treated as passing.

### Manual UI evidence changes

For a PR that changes the Windows manual UI evidence contract:

- checklist IDs and required screenshot coverage remain explicit;
- evidence binds to the exact accepted commit;
- the screenshot verifier checks PNG signature, dimensions, size, hashes, symlinks, traversal, and file allow-list;
- the tool does not capture screenshots, automate UI coordinates, inspect pixels, or upload evidence;
- privacy review and runtime-boundary attestation remain mandatory;
- generated screenshots are kept beneath `build/` or outside the repository and are not committed;
- implementation does not claim the actual Windows pass unless reviewed evidence exists.

## After merge

- verify merge commit on `main`;
- synchronize `foa-development` to the merge commit;
- verify post-merge workflows;
- update issue/milestone state;
- update roadmap phase when applicable;
- monitor regressions;
- revert promptly if safety or data integrity is compromised.

## Monthly repository health

- review open security reports;
- review stale high-priority issues and PRs;
- check dependency advisories;
- check broken documentation links;
- check CI duration, failures, and runner availability;
- confirm CODEOWNERS and templates remain accurate;
- review unsupported schema and migration backlog;
- review known compatibility and save-impact gaps;
- remove obsolete generated artefacts and branches;
- confirm only `main` and `foa-development` are long-lived.

## Before a release

Use `RELEASE_PROCESS.md` and additionally confirm:

- release commit is reviewed `main` state;
- version and tag are correct;
- compatibility matrix is current;
- changelog and release notes are complete;
- schemas and migrations are tested;
- packages contain no game assets, private data, or secrets;
- checksums are generated and verified;
- known limitations are prominent;
- upgrade and rollback steps are tested;
- public artefacts open successfully after upload.

For a Windows Developer Preview claim, additionally confirm:

- manual UI evidence was captured from the exact accepted commit;
- every checklist item passed;
- the screenshot verifier passed against that commit;
- screenshot hashes and tester alias are recorded in the review evidence;
- human privacy review is recorded;
- no proprietary content, game files, saves, credentials, or private paths are visible;
- the evidence directory and screenshots were not committed;
- any blocked or failed observation prevents the release claim.

## Emergency response

- minimise user harm first;
- preserve evidence and audit trail;
- disable or revert unsafe functionality;
- avoid publishing exploit details prematurely;
- coordinate private review;
- publish clear user action when safe;
- add regression tests and post-incident documentation;
- review whether architecture or governance must change.
