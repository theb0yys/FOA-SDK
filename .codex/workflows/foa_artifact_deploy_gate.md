# FOA-SDK Artifact and Deployment Review Gate

Use this workflow when a change can produce, package, install, copy, deploy, sign, or publish FOA-SDK artifacts. It is not applicable to source-only work that cannot affect artifact output.

## Rule D0: Identify the affected artifact set

Build or generate the product components required by the changed surface and its owning design. Do not demand unrelated products, but do not stop at a touched subtarget when shared dependencies make a broader artifact set applicable.

Generated output belongs under `FOA_BUILD_ROOT` or another reviewed external output directory. It is not source truth.

## Rule D1: Build from current source

Every claimed artifact must come from the current branch and exact pinned dependency/toolchain state. Do not reuse stale output as evidence for a changed source head.

If the required affected artifact set cannot be identified or built, report `PARTIAL` or `BLOCKED`.

## Rule D2: External writes require explicit authority

Before any write to an external conversion project, installer staging area, game installation, deployment location, signing service, or publication target:

- confirm source and destination;
- preserve backup/rollback or recovery paths where mutation can occur;
- record the current-task authority for the external operation;
- protect external/proprietary data.

A preview, manifest, plan, installer selection, or work order does not itself grant deployment authority.

## Rule D3: Verify applicable artifacts

Record applicable artifact paths, identities/hashes, configuration, source commit, dependency identity, and validation result. After an authorized copy or deployment, compare the evidence required by the owning design.

## Rule D4: Optional planning helper

Use `.codex/scripts/Get-AgentBuildDeployPlan.ps1` when artifact ownership, required products, destinations, or evidence are unclear. Its output is guidance, not authority to create or deploy unrelated artifacts.

## Rule D5: Handoff

Report the applicable build/generation commands, resulting artifacts, external destinations and rollback/recovery evidence when an external operation ran, and every `NOT_RUN` or `NOT_APPLICABLE` operational lane.
