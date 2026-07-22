# Windows Installer and Prebuilt Artifact Workflow Design

## Status

Approved design. The maintainer approved **Installer Design 1** and explicitly
authorised the dedicated `installer` branch on 21 July 2026. GitHub Issues are
disabled for this repository, so this versioned document and the installer
branch's draft pull request are the durable design-review record.

Approval authorises implementation and controlled development-channel testing.
It does not approve a public release, redistribution of an unreviewed inventory,
code signing, automatic update, FoA deployment, game launch, or save mutation.

## User problem

Mod authors must not have to clone this O3DE fork, configure CMake, install Visual
Studio, download the build dependency graph, or compile the Editor before using
the TG SDK. Contributors may retain the source-build path, but end users need a
standard prebuilt Windows installation and a portable recovery artifact.

## Accepted outcome

One reviewed Windows x64 `profile` build produces:

- a self-contained `FOA-SDK-Installer.exe` wizard with the exact reviewed MSI
  and its canonical SHA-256 record embedded;
- a per-user MSI with Start Menu integration and standard Windows Installer
  repair, upgrade, and uninstall behavior;
- a deterministic portable ZIP for controlled testing and recovery;
- one canonical `INSTALL_MANIFEST.json` shared by both forms;
- `SHA256SUMS`, a ZIP checksum, build provenance, third-party notices, the O3DE
  package inventory, and an SPDX 2.3 file inventory;
- a dedicated launcher that opens the installed Editor with the installed
  `TaintedGrailModdingEditor` project;
- no requirement for Git, Python, CMake, Visual Studio, or a source build on the
  user's machine.

The MSI is per-user so the editor project and O3DE user data have a writable
home without administrator elevation. Workspaces, FoA diagnostics, generated
outputs, staging roots, and deployment roots remain external user-selected data
and are never owned or removed by the installer.

## Owning layers and build layout

O3DE's `INSTALL` target owns the prebuilt SDK layout. It is configured with:

```text
LY_PROJECTS=<dedicated editor project>
LY_DISABLE_TEST_MODULES=ON
O3DE_INSTALL_ENGINE_NAME=TaintedGrailFoASDK
CMAKE_INSTALL_PREFIX=<isolated SDK install root>
```

The installer tooling does not scan or package raw `bin/profile` output. The raw
build contains tests, unrelated targets, and intermediate/debug material that is
not an install contract. The O3DE `INSTALL` target supplies the supported Editor,
runtime dependencies, Gem assets, SDK headers, CMake metadata, licences, and
installed-engine identity.

The repository-owned staging tool adds only the runtime portions of the
dedicated editor project (`project.json`, icon, preview, `Levels`, and
`ShaderLib`). Source CMake files and the project's transient `user` directory are
not copied as project payload. The installed project binding is rewritten to the
installed engine name without modifying the source document.

## Controlled pipeline

```text
validate
  -> configure Windows x64 profile SDK
  -> build O3DE INSTALL target
  -> generate notices and package inventory
  -> inventory exact files and SHA-256 values
  -> human redistribution review
  -> bind approval to the exact inventory fingerprint
  -> stage and re-hash the captured bytes
  -> create deterministic portable ZIP
  -> create MSI from the same staging root
  -> embed the reviewed MSI in the self-contained executable wizard
  -> checksum the final executable
  -> clean install / launcher self-test / repair / uninstall through the wizard
  -> retain unsigned development artifacts for review
```

The manual workflow has separate `inventory` and `package` modes. `package` mode
requires the exact prior inventory SHA-256, named reviewer, exact UTC review
time, and durable evidence reference. A rebuilt or changed inventory fails
closed and requires a new review. The workflow uploads development artifacts but
does not publish a GitHub release.

## Identity, manifest, and path rules

The install manifest schema is `1`. It records product/version/channel, exact
source commit, `profile` configuration, installed engine name, exact inventory
fingerprint, redistribution review, and sorted per-file path, size, SHA-256, and
source kind.

The inventory command derives `HEAD` from the repository and rejects a dirty
worktree; callers cannot select the source commit recorded in the artifact.

All payload destinations use Windows path identity even when tests run on a
case-sensitive host. The stager rejects absolute paths, `..`, control and illegal
characters, reserved device names, trailing spaces/dots, case aliases,
symlinks, junctions, reparse points, undeclared files, and destination
collisions. It writes a new sibling staging directory and publishes it only
after complete verification. Existing staging output is never overlaid.

`INSTALL_MANIFEST.json` and `SHA256SUMS` are self-excluded metadata: the manifest
hashes every other payload file, and `SHA256SUMS` hashes those files plus the
manifest. The portable ZIP has one versioned root, deterministic ordering and
timestamps, Zip64 support, and its own SHA-256 sidecar.

## Installer identity and lifecycle

`FOA-SDK-Installer.exe` is the standard user entry point. It is a native Windows
Forms executable published self-contained for Windows x64, so the user does not
need Python or a separately installed .NET runtime. It embeds the exact MSI and
checksum produced after redistribution review, extracts the MSI into a private
temporary directory, verifies the captured bytes, displays the selected
lifecycle operation and fingerprint, and invokes Windows Installer. An
adjacent or explicitly selected MSI is a development fallback only and requires
its canonical `.sha256` sidecar.

The executable does not replace MSI ownership or implement an independent file
copier. Windows Installer remains the sole authority for product-file mutation,
registration, Start Menu integration, repair, major upgrade, and uninstall.
The executable itself requests `asInvoker`; the reviewed MSI is per-user.

The MSI uses CPack's WiX generator with:

- fixed project-owned Upgrade Code
  `EF05F481-EEED-4CE5-A623-0915EC28F92D`;
- Product Code derived deterministically from that namespace and the exact
  three-component version;
- Windows x64 architecture and per-user install scope;
- one Start Menu entry for `TaintedGrailModdingEditorLauncher.exe`;
- Programs and Features metadata, standard repair, and uninstall.

The stable Upgrade Code and version-specific Product Code provide the required
major-upgrade identity. An actual older-to-newer MSI smoke requires two
independently reviewed inventories and remains a release gate until two versions
exist; it must not be inferred from structural configuration alone.

Uninstall removes product-owned files only. The smoke test creates an external
workspace sentinel and requires it to survive uninstall. Real user workspaces
must remain outside the install directory.

## Toolchain and supply-chain boundary

CMake/CPack is the package generator. WiX `4.0.4` and its matching UI extension
are pinned build-only tools. They are restored into isolated build caches and
are not placed in the user payload. The workflow records O3DE package licences
through the repository's license scanner before inventory review.

This design does not declare every generated binary redistributable. Exact
inventory and notice review is mandatory for every package build. Public release
also remains blocked by the repository's hosted-CI, exact-main, manual Editor UI,
security, legal, checksum, and signing gates.

## Failure and rollback

Every missing layout file, wrong engine identity, malformed version/commit/time,
unreviewed inventory, source change during staging, unsafe path, hash mismatch,
extra file, archive mismatch, package failure, executable-wizard build or
payload-verification failure, install failure, launcher
self-test failure, repair failure, or uninstall failure exits non-zero.

Rollback during development is deletion of the isolated build/install/staging
outputs and revert of the focused installer commit. Installed rollback uses
Windows Installer uninstall or installation of a separately reviewed newer
corrective version. No workflow deletes external workspaces or alters FoA.

## Explicit exclusions

- automatic updater or background service;
- signing keys, signing, or signature-verification claims;
- public release/tag creation or publication;
- automatic prerequisite installation beyond MSI itself;
- FoA discovery, deployment, BepInEx/Harmony loading, game launch, or save access;
- telemetry;
- proprietary game/toolkit files;
- bundled personal workspaces, logs, diagnostics, or private paths.

The official Merlin Workshop toolkit remains controlled research input only and
is not modified, copied, or redistributed by this installer.
