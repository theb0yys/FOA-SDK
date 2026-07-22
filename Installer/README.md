# FOA-SDK installer

`Installer/` owns the source needed to turn one reviewed prebuilt FOA-SDK payload into user-facing Windows artifacts. End users install the complete SDK from `FOA-SDK-Installer.exe`; the user workflow requires no Git, Python, CMake, Visual Studio, repository checkout, or O3DE source build.

Generated MSI files, portable ZIP archives, staged payloads, build caches, logs, screenshots, signing material, and release uploads belong under the external `foa-build/` root or another reviewed output directory, never in this source tree.

## Layout

```text
Installer/
├── Launcher/    installer-wizard and installed-Editor launcher source
├── Packaging/   MSI packaging source for a verified staging payload
└── Tests/       installer contract tests
```

The removed suite catalogue, receipt, capability-token, and Python bootstrapper lanes were non-installing review prototypes. They are not part of the approved prebuilt SDK delivery path and must not be restored as an alternative installer.

## User flow

The package workflow builds the canonical O3DE `INSTALL` target once, records and reviews its exact inventory, stages and re-hashes those captured bytes, and produces an MSI and portable ZIP from that same staging root. It then embeds the reviewed MSI and its SHA-256 sidecar into a self-contained `FOA-SDK-Installer.exe`.

The executable wizard:

1. extracts the embedded MSI into a private temporary directory;
2. verifies the captured MSI against the embedded canonical lowercase SHA-256;
3. displays the operation, installation directory, fingerprint, and external-workspace boundary;
4. invokes Windows Installer for install/upgrade, repair, or uninstall;
5. records the verbose Windows Installer log; and
6. optionally launches the installed `TaintedGrailModdingEditorLauncher.exe` after success.

An explicitly supplied or adjacent development MSI is accepted only with its `.sha256` sidecar. The launcher captures those bytes into its private temporary root and verifies the captured copy before execution, preventing a path from being changed between validation and Windows Installer startup.

The MSI is the lifecycle authority. It owns product files, Programs and Features registration, Start Menu integration, repair, major upgrade, and uninstall. Workspaces, FoA diagnostics, imported evidence, generated output, staging, deployment roots, and game files stay outside the installation directory and are never installer-owned.

## Boundaries

Installer selection or installation does not grant runtime execution, deployment, save mutation, signing, publication, catalogue mutation, or evidence-promotion authority. The installer does not discover, modify, launch, or deploy to FoA; install prerequisites beyond the reviewed MSI; contact an update service; or redistribute proprietary game or toolkit files.

Artifacts are unsigned development output until the separate signing and public-release gates are completed. SHA-256 proves byte integrity relative to the reviewed metadata; it is not a publisher signature.

## Acceptance

Installer changes require, as applicable:

1. repository structure and installer-workflow validation;
2. Windows installer-wizard contract tests and a warning-free Release build;
3. exact inventory, provenance, licence, notice, SBOM, and redistribution review;
4. successful MSI and portable ZIP creation from one verified stage;
5. executable-wizard construction with the exact reviewed MSI and checksum embedded;
6. clean install, installed-launcher self-test, repair, and uninstall through the executable;
7. proof that an external workspace sentinel survives repair and uninstall;
8. an independently reviewed second version before claiming actual upgrade coverage;
9. Windows Editor UI validation; and
10. the repository's exact-head validation and draft-PR process.

See [Windows Installer and Prebuilt Artifact Workflow Design](../docs/tainted-grail-sdk/WINDOWS_INSTALLER_AND_ARTIFACT_WORKFLOW_DESIGN.md) for the governing design and [Installing the Prebuilt Windows SDK](../docs/tainted-grail-sdk/INSTALLING_PREBUILT_SDK.md) for the user workflow.
