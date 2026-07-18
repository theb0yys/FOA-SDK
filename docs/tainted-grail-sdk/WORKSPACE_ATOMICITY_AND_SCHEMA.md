# Atomic Workspace Transition and Schema Contract

## Status

Accepted correction contract for Slices 3 and 4. It changes the editor-owned workspace document and load boundary only. It does not authorize runtime, deployment, game launch, telemetry, save mutation, or the Core/Framework build split.

## Durable workspace schema

`*.tgworkspace.json` uses durable schema version 1.

```json
{
  "SchemaVersion": 1,
  "WorkspaceId": "owner.workspace",
  "DisplayName": "Workspace",
  "RootPath": ".",
  "OutputPath": "Build",
  "StagingPath": "Staging",
  "DeploymentPath": "Deployment",
  "ActiveGameProfileId": "foa.mono.current",
  "GameProfiles": []
}
```

A document without `SchemaVersion` is legacy schema 0. The loader migrates it only when every schema-1 invariant can be validated without guessing. Unsafe legacy documents fail with an explicit schema-0 migration error. Unknown versions are rejected.

Every schema-1 workspace requires a lowercase namespaced stable `WorkspaceId`, non-empty workspace paths, at least one configured game profile, lowercase namespaced and unique `ProfileId` values, and an `ActiveGameProfileId` that binds to exactly one profile. `RuntimeTarget` is `Mono` or `IL2CPP`; Mono profiles also require the BepInEx version and plugin path.

A relative workspace root is resolved from the canonical workspace-document directory. Output, staging, deployment, diagnostics and extracted-data paths remain inside that root. Managed assemblies and the Mono plugin path remain inside the canonical game installation path.

## Atomic transition

`FoundationWorkspaceLoadService` creates a temporary `FoundationWorkspaceLoadCandidate` containing the migrated workspace, canonical document and root paths, validated active profile, rebuilt source/evidence registry, import issues, validated catalog and canonical catalog path.

Candidate construction executes in this order:

1. load and migrate the workspace document;
2. resolve and validate canonical workspace paths;
3. validate the active profile binding;
4. load source and evidence documents;
5. reject error-severity import issues;
6. rebuild the registry and validate source/evidence cross-bindings;
7. load the catalog when present;
8. validate workspace/profile/catalog bindings and rebuild the catalog database.

`FoundationService::LoadWorkspace` publishes only after every stage succeeds. Publication replaces the workspace, document path, canonical root, registry, import issues, catalog and catalog path, then builds one new snapshot. Failure publishes nothing and leaves all previous objects, paths, packs and the previous snapshot unchanged.

Candidate loading does not update the persistence boundary's published path. Pack containment reads the live `FoundationService` workspace path, so a failed candidate cannot redirect a later pack operation.

## Test evidence

Direct `FoundationService` integration tests inject failures at workspace loading, active-profile validation, path validation, source loading, import issues, registry binding, catalog loading, catalog binding and catalog database validation. Every failure compares the complete old live-state signature before and after the attempted transition.

Workspace persistence tests cover schema-1 round trips, unknown-version rejection, unsafe schema-0 rejection, and migration plus round trip of the project-owned Developer Preview fixture.

## Rollback

Revert the implementing pull request. Schema-1 documents remain ordinary UTF-8 JSON. Downgrading to a build that predates schema 1 is not a supported migration direction.
