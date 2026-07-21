## Summary

<!-- What problem does this solve? What user-visible or contributor-visible behavior changes? -->

## Linked issue or design review

<!-- Link the issue/design. Significant changes require approval before implementation. -->

Closes #

## Scope

<!-- List the files, services, tools, schemas, or workflows intentionally changed. -->

## Out of scope

<!-- State what this PR deliberately does not do. -->

## Architecture and product boundary

- [ ] O3DE remains the authoring host and FoA remains a separate runtime target.
- [ ] No FoA gameplay mutation, BepInEx loading, Harmony patch execution, save write, deployment, or runtime permission was added to editor/knowledge code.
- [ ] Domain logic remains in services rather than UI widgets.
- [ ] New dependencies or upstream O3DE changes are explained below.

Architecture notes:

## Identity, ownership, and evidence

- [ ] Native references and GUIDs remain exact.
- [ ] Display names are not used as identity keys.
- [ ] Synthetic/custom records are pack-owned.
- [ ] Source, evidence, claims, reviewed records, validation, and permission remain separate.
- [ ] Missing proof fails closed.

Identity/evidence notes:

## Data formats and migration

- [ ] No durable schema changed.
- [ ] Durable schema changed and the version, migration/rejection behavior, fixtures, and documentation are included.
- [ ] New documents remain inside approved workspace roots.
- [ ] Multi-file persistence publishes state only after required writes succeed.

Schema versions affected:

Migration or compatibility notes:

## Security, privacy, and legal review

- [ ] Imported or user-controlled input is bounded and validated.
- [ ] Path traversal, symlinks, case sensitivity, and platform path rules were considered when relevant.
- [ ] No secrets, credentials, personal paths, proprietary game data, or copyrighted assets are committed.
- [ ] Third-party licence/provenance was reviewed when relevant.
- [ ] Security-sensitive details are not disclosed publicly.

Security/legal notes:

## Save, deployment, and rollback impact

- [ ] No save, deployment, process-launch, or rollback behavior changed.
- [ ] Impact and rollback are described below and received required design review.

Impact:

Rollback/revert plan:

## Testing and validation

<!-- Include exact commands, configurations, fixtures, and manual steps. -->

- [ ] The automatic PR static-validation workflow passed for this exact head.
- [ ] Reviewed-range `git diff --check <base> HEAD` passed.
- [ ] Relevant Python, mutation, malformed-input, and negative tests passed.
- [ ] Persistence round-trip/migration tests passed when applicable.
- [ ] O3DE prerequisites and configure passed for the applicable host.
- [ ] O3DE build completed for required targets.
- [ ] compiled `TaintedGrailModdingSDK.Catalog.Tests` executed and passed; zero matching tests is an error.
- [ ] Manual editor/UI workflow verification passed, or the permitted risk acceptance is explicit.

Results:

## Exact-head validation receipt

- [ ] The receipt was initialized outside the repository from a clean exact HEAD.
- [ ] Every automated gate was executed by `validation_receipt.py record -- <command>`.
- [ ] `validation_receipt.py verify --require-merge-ready` passed for the reviewed 40-character HEAD.
- [ ] `validation_receipt.py summarize --require-merge-ready` output is included below.
- [ ] Any Windows UI skip and local maintainer risk declaration are explicit.

Do not commit the receipt or logs. Paste the verified summary here; local
receipts are tester-supplied evidence, not authenticated GitHub CI attestation.

Receipt summary:

## User-interface evidence

<!-- Add screenshots or steps for UI changes. Remove private paths and proprietary content. -->

Not applicable / Evidence:

## Performance impact

<!-- Describe scans, hashing, indexing, caching, memory, UI-thread, or build-time impact. -->

## Documentation

- [ ] Root README updated when public capability changed.
- [ ] User Guide updated for user-facing behavior.
- [ ] Data Formats updated for schema changes.
- [ ] Architecture updated for boundary changes.
- [ ] Changelog updated.
- [ ] Roadmap status updated when appropriate.

## Author self-review

- [ ] I reviewed the complete diff before committing.
- [ ] Commits are focused and DCO-signed.
- [ ] Direct includes and ownership are explicit.
- [ ] Debug code, secrets, local paths, and unrelated changes are removed.
- [ ] Errors and blockers are actionable.
- [ ] Required checks are complete; pending is not treated as passing.

## Reviewer checklist

- [ ] Scope and design match the linked issue.
- [ ] Architecture and runtime boundary are preserved.
- [ ] Identity, evidence, validation, and permission rules are correct.
- [ ] Schemas, migrations, compatibility, and rollback are adequate.
- [ ] Security and failure paths are tested.
- [ ] Static CI and exact-head host evidence belong to this head.
- [ ] Documentation matches implementation.
- [ ] All blocking threads are resolved before merge.
