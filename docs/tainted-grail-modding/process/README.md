# Mod-Development Process

This lane adapts the proven source-repository lifecycle to current FOA-SDK governance and tooling.

## Canonical lifecycle

```text
research
→ design
→ scaffold
→ implement
→ self-review
→ independent review
→ static and compiled validation
→ controlled runtime validation
→ package review
→ release decision
→ support and compatibility maintenance
```

## 1. Research

Record the exact game version, branch, runtime, loader, assembly/type/member candidates, behavior evidence, compatibility risks, source provenance, uncertainty, and prohibited operations. Stop before implementation when the target is unresolved.

## 2. Design

Define the user outcome, data and runtime ownership, hook or API surface, failure behavior, performance and story/save risks, dependencies, config, diagnostics, compatibility, cleanup, rollback, evidence plan, and release boundary.

## 3. Scaffold

Use a project-owned reviewed example or template. Set unique plugin identity, assembly and namespace, exact dependencies, configurable private reference paths, documentation, compatibility profile, validation plan, and package metadata.

## 4. Implement

Prefer small reversible changes, narrow patches, explicit null/failure guards, stable config, bounded logging, clean registration/unregistration, no hidden network or filesystem authority, and no unreviewed hot-path work.

## 5. Review

Review findings first. Check target correctness, lifecycle, ordering, cleanup, config migration, performance, interoperability, package freshness, private/proprietary content, and whether the evidence supports the claims.

## 6. Validate

Separate evidence layers:

- documentation and source checks;
- build and compiled tests;
- exact target/signature verification;
- loader/plugin startup;
- bounded behavior verification;
- combined-mod and load-order testing;
- UI/accessibility evidence where applicable;
- save/rollback verification where applicable.

Missing evidence remains pending, not passing.

## 7. Package and release

Package only required redistributable files. Rebuild after the final source, identity, dependency, or package change. Bind release metadata to exact inputs and outputs. Do not include game/loader binaries, private config, saves, logs, credentials, or unreviewed assets.

## 8. Support and maintenance

Collect reproducible profiles and safe diagnostics, verify reports before changing compatibility records, preserve superseded profiles, document migrations, and retire stale hooks explicitly.

## FOA-SDK integration

When applicable, the process also requires:

- canonical catalog and evidence intake;
- governed candidate-evidence submission rather than automatic promotion;
- public ExtensionAPI use instead of mutable Foundation internals;
- reviewed provider and adapter contracts;
- deterministic installer/package planning;
- exact-head validation and receipts;
- explicit separation of editor authoring from target-game runtime authority.

Detailed checklists will be migrated only after reconciling them with the current repository review and merge policy.