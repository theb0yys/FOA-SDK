# Actor and Troop Release Readiness

Status: documentation and deterministic fixture complete; exact-head host evidence pending.

## Purpose

This document defines the development, review, and release-readiness gates for the Actor and Troop Editor vertical slice. It supplements the repository development and release processes without weakening their requirements.

A complete source diff is not a release-ready editor. Acceptance requires the same exact commit to pass contract validation, compiled tests, a real O3DE Windows build, Editor interaction, persistence smoke, and reviewed evidence.

## Implemented scope

The reviewed slice includes:

- typed actor, troop, and membership models;
- catalog schema-2 migration and durable writing;
- exact evidence and identity validation;
- atomic actor-profile and troop-definition candidate publication;
- immutable seven-lane population action decisions;
- registered Actor/Troop Editor pane with dirty-draft protection;
- project-owned deterministic population fixture;
- focused validators and positive/negative tests;
- public user, architecture, format, and evidence guidance.

It does not include runtime spawning, encounters, deployment, game-process access, save mutation, or FoA compatibility certification.

## Development gate

Before requesting review:

1. synchronize `foa-development` with current `main`;
2. confirm the diff contains only Actor/Troop scope and necessary shared documentation;
3. run the authoritative local validator;
4. generate and verify the deterministic population fixture;
5. run focused Python tests;
6. configure an O3DE build and run compiled catalog tests;
7. perform the Windows Editor checklist on the exact head;
8. attach only reviewed external receipts and screenshot metadata;
9. update roadmap, changelog, architecture, data formats, user documentation, and release notes together;
10. DCO-sign every commit.

## Required local commands

From a real repository checkout:

```powershell
python Gems/TaintedGrailModdingSDK/Tools/run_local_validation.py --keep-going
```

Generate and verify the population fixture independently:

```powershell
python Gems/TaintedGrailModdingSDK/Tools/population_preview_fixture.py generate `
  --output build/tg-sdk-population-preview-fixture
python Gems/TaintedGrailModdingSDK/Tools/population_preview_fixture.py verify `
  --output build/tg-sdk-population-preview-fixture
```

Run the focused fixture tests:

```powershell
python -m unittest `
  Gems.TaintedGrailModdingSDK.Tools.tests.test_population_preview_fixture -v
```

Repository test discovery may be used instead when module naming differs in the local shell:

```powershell
python -m unittest discover `
  -s Gems/TaintedGrailModdingSDK/Tools/tests `
  -p "test_population_preview_fixture.py" -v
```

## Exact-head O3DE build gate

Use the commands in `DEVELOPER_PREVIEW_EXACT_HEAD_VERIFICATION.md` and record the exact 40-character commit before configuration.

The minimum host evidence is:

- successful O3DE configure for the dedicated TG SDK project;
- successful `AssetProcessorBatch.exe` and `Editor.exe` Profile build;
- successful compiled `TaintedGrailModdingSDK.Catalog.Tests` execution;
- successful fixture generation and verification from the same checkout;
- successful controlled Editor launch;
- activation log proving the TG SDK Gem and pane system components activated;
- no unreviewed local source changes.

A configure/build result from another commit, another branch, or an unrecorded working tree is not evidence for the reviewed head.

## Compiled test gate

The compiled suite must exercise production-linked code rather than recompiling implementation files into the test target.

Required population coverage includes:

- schema-1 migration and schema-2 persistence;
- actor resolved and unresolved template bindings;
- troop resolved and unresolved leader bindings;
- member role, count, weight, exact-subject, and duplicate rejection;
- exact evidence coverage for every required subject;
- optional-member zero-minimum behavior;
- leader-row/profile agreement;
- aggregate member-range overlap;
- atomic publication and notification;
- persistence failure leaving the published catalog unchanged;
- immutable action-lane ordering and runtime/save lanes unavailable.

Record the command, configuration, test count, result, exact commit, and log location.

## Windows Editor evidence gate

The current product surface contains twenty-six TG SDK panes because the FOA Development Hub is the default entry and routes to twenty-five specialist panes, including the optional Road Atlas and Avalon AI authoring Tool Gems. Actor/Troop is one of those specialist panes.

The Actor/Troop evidence pass must prove:

- the pane opens from both the Tools menu and Hub route;
- the deterministic population workspace loads;
- both actors and the patrol troop are visible;
- the resolved guard template and unresolved scout template subject are distinguishable;
- exact leader and member bindings are visible;
- allowed and forbidden `spawn_candidate` states are visible;
- `runtime_spawn` and `save_mutation` remain unavailable;
- dirty actor, troop, and member drafts block destructive selection changes;
- Foundation refresh is deferred while drafts are dirty;
- save, close, and reopen preserves schema-2 population state;
- a safe invalid member range or evidence mismatch produces an actionable error;
- no runtime, deployment, game launch, or save-mutation control exists.

Screenshots must use only project-owned synthetic data and must not be committed to the repository.

## Evidence record

For each exact-head verification, record:

| Evidence | Required value |
| --- | --- |
| source commit | exact 40-character SHA |
| branch | `foa-development` or reviewed PR head |
| working tree | clean |
| Windows version | explicit version/build |
| display scale | 100–200% |
| configure | command and exit code |
| build | targets, configuration, and exit code |
| compiled tests | command, count, and result |
| local validation | command and result |
| fixture | manifest ID and verification result |
| Editor launch | exit/start result and log path |
| pane coverage | all 26 panes plus Actor/Troop checks |
| persistence | save/close/reopen result |
| privacy review | completed |
| runtime boundary | confirmed absent |

## Release decision

The Actor/Troop slice may be described as implemented only when source, deterministic fixture, tests, and public documentation are present.

It may be described as exact-head verified only when the same commit has the complete host-build, compiled-test, and Windows evidence above.

It may not be described as FoA runtime compatible, production ready, or release ready merely because the pane opens or the fixture passes. Runtime adapters, game-version evidence, deployment, encounter execution, and save behavior remain separate gates.

## Rollback

A failed documentation or fixture unit can be reverted without catalog migration because it does not change the durable schema beyond the already reviewed schema-2 contract.

A failed source implementation must be reverted as a complete vertical slice. Do not retain a pane that writes data the current loader rejects, or a schema writer without corresponding migration, tests, and documentation.

## Roadmap and changelog state

After this unit:

- deterministic population fixture and public documentation are complete;
- the Actor/Troop pane remains in active hardening until exact-head host evidence is recorded;
- Spawn and Encounter Editor remains the next population-domain authoring capability;
- runtime population adapters remain deferred;
- no release claim is made from documentation alone.
