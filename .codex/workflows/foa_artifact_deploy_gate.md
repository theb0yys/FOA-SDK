# FOA-SDK Artifact and Deployment Review Gate

This workflow is mandatory after coding changes that can produce FOA-SDK Editor binaries, plug-in packages, installer artifacts, Unity conversion outputs, or runtime-adapter binaries.

## Rule D0: Full Relevant Artifact Set Required

Do not stop at source edits, tests, or a touched-project build. Build the latest reviewed configuration for every affected product component and dependency lane identified by preflight.

Generated output belongs under `FOA_BUILD_ROOT` or another reviewed external output directory. It must not become source truth.

## Rule D1: Build From Current Source

Every claimed artifact must come from a fresh build of the current branch and exact pinned O3DE/toolchain state. Do not reuse stale output.

If the complete affected artifact set cannot be identified or built, report the gate as blocked or partial.

## Rule D2: Backup Before Any External Write

Before any explicitly authorised write to an external Unity conversion project, installer staging area, game installation, or deployment location:

- confirm every source and destination;
- preserve backup or rollback paths;
- record the approval authorising the external write;
- do not overwrite protected external data.

No editor, plug-in, installer selection, or work order grants deployment authority by itself.

## Rule D3: Verify Artifacts

Record artifact paths, SHA256 hashes, timestamps, configuration, source commit, pinned dependency identity, and validation receipts. After an authorised copy, compare source and destination hashes and timestamps.

## Rule D4: Outside-Repository Writes Need Explicit Approval

External writes require explicit current-task permission and any environment escalation. If approval or access is absent, report deployment as not run, blocked, or partial.

## Rule D5: Handoff

Every final response after an artifact-producing change states:

- full relevant build commands;
- artifact paths;
- reviewed external destinations, if any;
- backup/rollback paths;
- hash/timestamp verification;
- whether Editor, Unity conversion, installer, or Fall of Avalon runtime sign-off was performed.
