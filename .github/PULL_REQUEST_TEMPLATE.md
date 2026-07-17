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

- [ ] `python Gems/TaintedGrailModdingSDK/Tools/validate_foundation.py`
- [ ] `git diff --check`
- [ ] Relevant unit tests
- [ ] Persistence round-trip/migration tests
- [ ] Malformed-input and negative tests
- [ ] O3DE configure/build for applicable host platform(s)
- [ ] Manual editor workflow verification

Results:

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
- [ ] Documentation matches implementation.
- [ ] All blocking threads are resolved before merge.
