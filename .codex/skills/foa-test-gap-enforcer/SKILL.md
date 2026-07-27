---
name: foa-test-gap-enforcer
description: Use for every FOA-SDK code, UI, O3DE host, plug-in, toolchain, Unity conversion, installer, runtime-adapter, migration, harness, test, or bug-fix change. It maps required proof to owner-specific lanes, prevents generic false-green validation, and forces missing tests to be added or reported as partial or blocked.
---

# FOA-SDK Test Gap Enforcer

Use after impact and compatibility classification for every implementation or validation task.

## First Actions

1. Run or reproduce `.codex/scripts/Get-AgentTestPlan.ps1`.
2. Identify owner systems and changed surfaces.
3. Capture the immediate local commands, O3DE host rows, static package and interchange assertions, manual Editor/Unity/installer rows, exact-install runtime rows, and non-runnable governed rows.
4. Compare required proof with existing tests and evidence lanes.
5. Classify every missing lane as add now, blocked, partial, or not applicable with authority.

## Research

Read:

- `.codex/workflows/foa_sdk_test_gates.md`
- owner research, contracts, schemas, and implementation
- existing unit, integration, malformed-input, migration, and harness tests
- O3DE configure/build and compiled-test requirements
- Unity conversion, installer, runtime-adapter, and release gates when applicable
- the evidence-pack and system-test-matrix templates

## Evidence Classes

Keep these classes separate:

1. Local runnable gates: linters, validators, Python tests, unit tests, static assertions.
2. O3DE host gates: prerequisites, configure, build, compiled tests, Asset Processor or Editor-host checks.
3. Manual product gates: Editor interaction, UI evidence, Unity conversion, installer execution, repair, upgrade, rollback, or packaging review.
4. Exact-install runtime gates: lawful Fall of Avalon profile and adapter execution evidence.
5. Non-runnable governed rows: required proof that cannot run in the current environment and must remain explicit.

A pass in one class cannot substitute for another.

## Surface-Specific Proof

Require distinct proof for:

- Foundation or owner-core behavior
- public contracts and schemas
- persistence and canonical interchange
- O3DE components, buses, panes, and host integration
- UI routes and assets
- plug-in registration and isolation
- external-tool discovery and process supervision
- Unity conversion and returned candidate evidence
- installer build, install, repair, upgrade, rollback, and uninstall
- Mono and IL2CPP adapter paths
- migration and release behavior
- test-harness truth and false-green prevention

## Harness Truth

The harness is evidence authority only. It must not become product or runtime sovereign.

- a successful test-project build is compilation evidence only
- a discovered descriptor is not executed proof
- a skipped or unbound governed row cannot support a green verdict
- harness defects must not be blamed on the product owner
- zero matching tests is an error when a compiled test target is required

## Hard Stops

Stop or report partial or blocked when:

- owner or required surface is unknown
- proof from one surface is substituted for another
- a required lane is missing and is neither added nor explicitly blocked
- configure or build success is presented as Editor, Unity, installer, adapter, deployment, or runtime proof
- static package assertions are omitted for package-sensitive work
- manual or exact-install rows are silently dropped
- missing tests are hidden behind a generic suite pass

## Validation

Report:

- required tests by owner and surface
- existing lanes
- missing lanes
- commands run and exact results
- static assertions
- O3DE host rows
- manual Editor/Unity/installer rows
- exact-install runtime rows
- non-runnable governed rows
- gap status: complete, partial, blocked, or not applicable

## Runtime Proof

Local tests and O3DE builds do not prove Fall of Avalon runtime behavior. Without a lawful exact-install runtime lane, state `runtime sign-off not performed`.
