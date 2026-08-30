## Summary

<!-- What problem does this solve and what changes? -->

## Change classification

Select exactly one.

- [ ] Routine <!-- change-classification:routine -->
- [ ] Significant <!-- change-classification:significant -->
- [ ] Critical/Runtime <!-- change-classification:critical-runtime -->

Classification rationale:

## Scope

<!-- Files, systems, APIs, schemas, workflows, or behavior intentionally changed. -->

## Out of scope

<!-- State what this PR deliberately does not do. -->

## Design / architecture impact

<!-- Required for Significant and Critical/Runtime changes. For Routine changes, write "Not applicable" when there is no design change. -->

## Compatibility, data, and rollback

<!-- Describe public API, schema, persistence, migration, dependency, deployment, or rollback impact when applicable. Otherwise state "Not applicable". -->

## Validation performed

List the **exact commands/checks that actually ran** and their results. Use the validation matrix in `docs/tainted-grail-sdk/CI_AND_LOCAL_VALIDATION.md`.

```text
PASSED:
FAILED:
NOT_RUN / NOT_APPLICABLE:
```

Do not mark host, compiled, UI, runtime, installer, deployment, or release evidence as required when the changed surface cannot affect that layer.

## Security / protected-data impact

<!-- Identify secrets, private paths, proprietary data, saves, installations, runtime permissions, or security boundaries if relevant. Otherwise state "None". -->

## Documentation

<!-- List docs updated, or explain why no documentation change is required. -->

## Author self-review

- [ ] The diff is focused on the stated scope.
- [ ] No secrets, private paths, protected game data, or generated build output were committed.
- [ ] DCO sign-off is present.
- [ ] Validation claims above describe only checks that actually ran.
- [ ] Significant/Critical design and migration/rollback implications are documented when applicable.

## Maintainer review

- [ ] Classification is appropriate.
- [ ] Required validation for the changed surface is satisfied.
- [ ] Blocking review findings are resolved.
- [ ] Documentation and compatibility notes are adequate.
