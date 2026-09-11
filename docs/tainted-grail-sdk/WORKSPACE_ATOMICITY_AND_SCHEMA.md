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

A document without `SchemaVersion` is legacy schema 0. The loader detects the schema before selecting either the legacy O3DE-envelope parser or the plain durable-document parser. It migrates schema 0 only when every schema-1 invariant can be validated without guessing. Unsafe or malformed legacy documents fail with an explicit schema-0 migration error. Explicit unknown versions are rejected even when a document also contains a legacy `Type` envelope marker.

Every schema-1 workspace requires a lowercase namespaced stable `WorkspaceId`, non-empty workspace paths, at least one configured game profile, lowercase namespaced and unique `ProfileId` values, and an `ActiveGameProfileId` that binds to exactly one profile. `RuntimeTarget` is `Mono` or `IL2CPP`; Mono profiles also require the BepInEx version and plugin path.

A relative workspace root is resolved from the canonical workspace-document directory. Output, staging and deployment paths remain inside that root. For every configured profile, diagnostics and extracted-data paths remain inside the workspace root, managed assemblies remain inside the canonical game installation path, and Mono plugin paths remain inside that same installation path.

## Atomic transition

### Automatic local setup

Foundation owns automatic game discovery and workspace registration. On Windows,
Steam's registered client location and its bounded library metadata provide install
candidates, including libraries on other drives. A manual game-folder selection
takes precedence over a saved install path and refreshes derived game paths and
version observations. The folder picker applies its selection through its completion
signal; canceling it does not change the workspace. Changing installations clears live workspace-scoped caches;
it does not rebind existing evidence to the new installation.

Fresh setup uses the per-user `FOA-SDK/Workspace` location. Legacy Tool Wizard
workspace hints are used only when they point to an existing workspace document,
and an existing automatic workspace takes precedence. A stale hint alone cannot
redirect a fresh registration. Existing workspace documents retain schema 1 and
the same configured-profile and path-containment requirements. Failed registration
preserves the previous workspace and reports the missing profile prerequisite or
persistence error. Discovery and registration never create game or loader files.

### Workspace document loading

`FoundationWorkspaceLoadService` creates a temporary `FoundationWorkspaceLoadCandidate` containing the migrated workspace, canonical document and root paths, validated active profile, rebuilt source/evidence registry, import issues, validated catalog and canonical catalog path.

Candidate construction executes in this order:

1. load and migrate the workspace document;
2. resolve and validate canonical workspace paths for the workspace and every configured profile;
3. validate the active profile binding;
4. load source and evidence documents;
5. reject error-severity import issues;
6. rebuild the registry and validate source/evidence cross-bindings;
7. load the catalog when present;
8. validate workspace/profile/catalog bindings and rebuild the catalog database.

`FoundationService::LoadWorkspace` publishes only after every stage succeeds. Publication replaces the workspace, document path, canonical root, registry, import issues, catalog and catalog path, then builds one new snapshot. Failure publishes nothing and leaves all previous objects, paths, packs and the previous snapshot unchanged.

After candidate validation, `BeginWorkspaceChange` asks trusted host handlers for
admission before clearing or replacing live state. Cancel or a failed draft Save
vetoes the switch; Save writes to the original workspace. A completed Save remains
saved if another handler later vetoes. After all workspace objects are published,
`FinishWorkspaceChange` refreshes the snapshot, sends `OnWorkspaceChanged`, and
releases the reentrancy guard. Pack Manager resets its draft only on that commit
notification. The atomicity validator checks this ordering across both functions;
mutation tests reject missing steps and premature publication or notification.

Candidate loading does not update the persistence boundary's published path. Pack containment reads the live `FoundationService` workspace path, so a failed candidate cannot redirect a later pack operation.

## Test evidence

Direct `FoundationService` integration tests inject failures at workspace loading, active-profile validation, path validation, source loading, import issues, registry binding, evidence binding, catalog loading, catalog binding and catalog database validation. Every failure compares the complete old live-state signature before and after the attempted transition.

Local-setup integration tests cover stale legacy workspace hints, manual install
replacement and derived paths, restart persistence, and rejected selections.
`Tools/editor_tests/game_location_live_smoke.py` exercises the actual status pane,
folder picker, save, and recheck through Editor Python. Its caller supplies isolated
`LOCALAPPDATA` and Editor user/log directories, the expected read-only game path,
and a result file using `FOA_SDK_GAME_LOCATION_EXPECTED`,
`FOA_SDK_GAME_LOCATION_MODE` (`auto`, `manual`, or `reopen`), and
`FOA_SDK_GAME_LOCATION_RESULT`. Manual mode starts with a different synthetic
installation saved in the isolated workspace; reopen mode uses its resulting
workspace in a fresh Editor process. Launch with `--runpython` and the smoke script;
the JSON result must report `PASSED`. Process exit alone is insufficient.

Workspace persistence tests cover schema-1 round trips, unknown-version rejection, unknown-version rejection through a legacy-envelope marker, malformed and unsafe schema-0 rejection, and migration plus round trip of the project-owned Developer Preview fixture.

Path-policy tests cover valid multi-profile workspaces, workspace-root escape, managed-assembly and Mono-plugin escape from the installation root, and invalid paths on inactive configured profiles.

## Rollback

Revert the implementing pull request. Schema-1 documents remain ordinary UTF-8 JSON. Downgrading to a build that predates schema 1 is not a supported migration direction.
