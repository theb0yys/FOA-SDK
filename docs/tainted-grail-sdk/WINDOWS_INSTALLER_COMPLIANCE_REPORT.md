# Windows Installer Design Compliance Report

## Authority and audited baseline

The sole requirements authority for this audit is
[`WINDOWS_INSTALLER_AND_ARTIFACT_WORKFLOW_DESIGN.md`](WINDOWS_INSTALLER_AND_ARTIFACT_WORKFLOW_DESIGN.md).
No requirement outside that approved document is treated as an installer acceptance gate.

The merged implementation audited clause by clause was `main` commit
`9203e75728292a5e0c18bebe7d48056fa0a8f95a`. Corrections are isolated in draft
pull request **#179**, branch `installer-design-conformance`.

This report distinguishes implementation compliance from artifact evidence. The
implementation is not declared finally complete until an exact-head inventory is
reviewed by a human and package mode produces and passes lifecycle testing for
`FOA-SDK-Installer.exe`.

## Genuine deviations found and corrected

| Deviation in audited `main` | Why it violated the approved process | Correction | Enforcement |
|---|---|---|---|
| The retained artifact directory was also passed to `cpack -B`. CPack necessarily creates `_CPack_Packages` below that directory. | The retained directory must contain only the exact EXE/MSI/ZIP artifact set and sidecars. The package workflow would always fail the final exact-set verifier or retain undeclared work files. | CPack now works in isolated `SDK_PACKAGE_OUTPUT`; only the generated MSI and sidecar are copied into `SDK_ARTIFACTS`. Commit `7c93011f90eaad693b8fb3314ab9444fb0d137ab`. | `test_cpack_work_area_is_separate_from_retained_artifacts`, `test_cpack_work_files_cannot_leak_into_retained_artifacts`, and `validate_installer_workflow.py` forbid `-B $env:SDK_ARTIFACTS`. |
| The repository had no complete final-artifact-set verification before lifecycle testing and retention. | The approved process requires final EXE checksum verification and fail-closed artifact retention. | Added `verify_installer_artifacts.py` and inserted it before lifecycle smoke. Commits `1f69f49be263ddcc317efc3ba3a2ac059feb06d4`, `85b6a552f457c61f8bd9bc4513e7a6a414cdc487`, and `8ff5f74c5c7ce7e5ba1293b5d7d862120f43487a`. | `test_complete_artifact_set_and_sidecars_verify`, checksum-tamper tests, exact-set tests, and the authoritative installer validator. |
| The executable fallback checksum parser accepted a hash-only record and arbitrary whitespace. | The design requires the embedded or adjacent MSI to use its canonical SHA-256 sidecar. | Parser now requires exactly `<lowercase sha256>  <installer.msi>` followed by one LF. Commits `2eb21c0011626263e82d150633990731c9ba6dd0` and `57c5faa9447480324ca160a7d3e80603f0f63f29`. | `test_embedded_msi_parser_requires_the_same_canonical_record` and `WindowsInstallerWizardTests.test_payload_is_embedded_or_adjacent_and_sha256_verified`. |
| Lifecycle smoke checked operation exit codes but did not prove the Start Menu target, shared installed manifest, or real repair restoration. | The design requires the same manifest payload, a launcher Start Menu entry, and working repair of product-owned files. | Smoke now compares installed/staged manifest hashes, resolves the shortcut target, damages the installed launcher, requires repair to restore its exact hash, and reruns launcher self-test. Commit `7c93011f90eaad693b8fb3314ab9444fb0d137ab`. | `test_lifecycle_smoke_proves_manifest_shortcut_repair_and_uninstall` plus the package-mode Windows smoke itself. |
| Bypass prevention rejected one known temporary workflow name only. | A renamed alternate workflow could bypass the canonical inventory/review/package route. | Validator now scans all other YAML workflows for installer execution signatures. Commit `00f5811d428d9e2e612d906df57361aae37285b6`. | `test_renamed_alternate_installer_workflow_is_rejected` and `test_temporary_inventory_workflow_is_rejected`. |
| Explicit exclusions were documented but not all were protected by a regression test over the installer-owned implementation. | Signing, update/service, FoA deployment, telemetry, prerequisite installation, and proprietary toolkit behavior must remain outside this installer. | Added scoped negative tests over the canonical workflow, packaging, wizard, runner, payload, and installed launcher. Commit `b0b69f1709324736f271aabd65d4e7aa787456f6`. | `test_explicitly_excluded_installer_capabilities_remain_absent`. |

## Clause-by-clause requirement mapping

| Research requirement | Implementing file and code | Enforcing validator or test | Remaining gap |
|---|---|---|---|
| Manual controlled workflow; no automatic trigger or public release | `.github/workflows/tainted-grail-sdk-installer.yml`: `workflow_dispatch`, manual `mode`, read-only permissions, artifact upload only | `validate_installer_workflow.py`; `test_public_release_command_is_rejected`; forbidden `push:` and `pull_request:` | None in implementation |
| Validate repository and required compiled tests before SDK packaging | Workflow: compiled validation configure/build followed by `run_local_validation.py --ctest-build-dir` | Pipeline-order test; canonical workflow validator; exact-head PR static/compiled checks | Exact-head checks must be green after the final correction commit |
| Configure Windows x64 `profile` prebuilt SDK | Workflow: VS 2022, `-A x64`, `--config profile`, tests disabled for install layout | Workflow validator and pipeline-order test | None in implementation |
| O3DE `INSTALL` owns the payload; raw build output is not packaged | Workflow: `--target INSTALL`, `CMAKE_INSTALL_PREFIX=$env:SDK_INSTALL`, both inventory and stage use `--sdk-root $env:SDK_INSTALL`; `developer_preview_installer.py::build_sources` scans the install root | `test_inventory_and_stage_consume_only_the_o3de_install_root`; installer validator; SDK identity tests | None in implementation |
| Dedicated project contributes only runtime project files and is rebound to the installed engine | `developer_preview_installer.py`: `PROJECT_FILES`, `PROJECT_DIRECTORIES`, `installed_project_json` | `test_inventory_is_deterministic_and_uses_installed_project_binding` | None |
| Generate redistribution notices and O3DE package inventory before review | Workflow license scanner step writes `NOTICES.txt` and `THIRD_PARTY_PACKAGES.json` before inventory upload | Pipeline-order test; stager rejects missing, empty, or malformed review inputs | Human legal/redistribution judgement remains mandatory |
| Inventory exact file paths, sizes, SHA-256 values, source kinds, version, channel, source commit, profile configuration, and engine identity | `developer_preview_installer.py::build_inventory` and `inventory_entry` | Deterministic inventory, schema, SDK identity, clean-head, and path-policy tests | None |
| Inventory derives a clean exact Git `HEAD`; caller cannot select source commit | `derive_clean_repository_commit` | `test_source_commit_is_derived_only_from_a_clean_git_head` | None |
| Human review supplies exact fingerprint, named reviewer, UTC time, and durable evidence | Workflow package inputs and `stage_payload` review validation | Exact-inventory approval test; manifest-schema validation; workflow validator | A real human must provide these values for the exact generated inventory |
| Rebuilt or changed inventory fails closed | Workflow compares approved and generated fingerprints; staging reconstructs inventory and rehashes copied bytes | `test_stage_requires_exact_inventory_redistribution_approval`; `test_stage_detects_input_drift_after_inventory` | None |
| Stage into a new sibling directory, reject overlays, unsafe identities, links, collisions, drift, undeclared files, and mismatches | `developer_preview_installer.py`: `validate_destination`, `iter_regular_files`, `copy_source`, `stage_payload`, `verify_stage` | Path alias/reserved-name tests, symlink test, tamper/undeclared-file tests, drift test | Windows junction/reparse behavior is additionally exercised on Windows hosts |
| Manifest and checksum metadata use the approved self-exclusion model | `stage_payload`, `manifest_entries`, `verify_stage` | Manifest/schema and stage verification tests | None |
| Generate build provenance, notices, package inventory, and SPDX 2.3 file inventory | `stage_payload` and `build_sbom` | `test_stage_manifest_and_portable_archive_verify` | None |
| Deterministic portable ZIP with one versioned root, stable order/timestamps, Zip64, and checksum sidecar | `archive_payload` and `verify_archive` | Portable archive verification and byte-determinism tests | None |
| MSI consumes the same verified staging root as the ZIP | Workflow passes `SDK_STAGE` to both archive and `TG_INSTALLER_PAYLOAD_ROOT`; packaging CMake installs that directory | `test_zip_and_msi_are_built_from_the_same_verified_stage`; installed-manifest hash comparison | Must be proven by the final package run |
| WiX 4.0.4 and matching UI extension are pinned build-only tools in isolated caches | Workflow installs WiX under `$RUNNER_TEMP/wix` and redirects the global extension cache with `$RUNNER_TEMP/wix-extensions` | Installer validator and `test_wix_extension_cache_must_be_isolated` | None |
| MSI is Windows x64 and per-user, with stable Upgrade Code, version-derived Product Code, Start Menu launcher, Programs and Features metadata, repair, and uninstall | `Installer/Packaging/Windows/CMakeLists.txt` | Installer validator; launcher/package source tests; package-mode lifecycle smoke | Actual older-to-newer upgrade smoke remains intentionally pending until two independently reviewed versions exist |
| `FOA-SDK-Installer.exe` is a self-contained Windows Forms x64 executable embedding the exact MSI and sidecar | `FOAInstallerLauncher.csproj`; workflow `dotnet publish`; `InstallerPayload.cs` | `WindowsInstallerWizardTests.test_project_builds_self_contained_winforms_exe_with_optional_embedded_msi`; package smoke resolves and verifies embedded resources | Must be built on the exact reviewed package run |
| Executable extracts into a private temporary directory and verifies captured MSI bytes | `InstallerPayload.cs::FromEmbeddedResource`, `FromExternalFile`, and `VerifyChecksum` | Windows installer launcher tests and canonical sidecar tests | Must pass in the final EXE smoke |
| Executable displays operation and reviewed fingerprint and delegates mutation only to Windows Installer | `InstallerWizardForm.cs`; `WindowsInstallerRunner.cs` uses `msiexec /i`, `/fa`, and `/x` | Windows installer wizard tests | None |
| Executable remains `asInvoker`; MSI remains per-user | `Installer/Launcher/Windows/app.manifest`; packaging CMake | Manifest and packaging validator tests | None |
| Final retention contains exactly EXE/MSI/ZIP plus canonical checksum sidecars | `verify_installer_artifacts.py`; isolated CPack output; workflow retention directory | Exact artifact-set, extra-directory, malformed-record, and checksum-tamper tests | Must pass against the generated artifacts |
| Install proves the MSI carries the exact reviewed manifest and Start Menu shortcut targets the installed launcher | Package-mode smoke in canonical workflow | `test_lifecycle_smoke_proves_manifest_shortcut_repair_and_uninstall` | Must pass on Windows package run |
| Repair restores exact product-owned launcher bytes | Package-mode smoke damages launcher, runs wizard repair, compares reviewed hash, and reruns self-test | Lifecycle smoke contract test | Must pass on Windows package run |
| Uninstall removes product-owned launcher, manifest, and shortcut but preserves external workspace data | Package-mode smoke and wizard boundary text | Lifecycle smoke contract test; Windows wizard boundary test | Must pass on Windows package run |
| Installed launcher opens the installed Editor with the installed project and has a non-launching self-test | `InstalledEditorLauncher.cpp` | Installer workflow validator and launcher source tests | Full manual Editor UI acceptance remains a separate release gate, as approved |
| End user does not need Git, Python, CMake, Visual Studio, source build, or separate .NET runtime | O3DE install payload, MSI, self-contained EXE, installation documentation | Windows wizard project/source tests and installer documentation validator | Must be confirmed by final clean-machine package smoke |
| No alternate or legacy installer route bypasses canonical inventory/review/package flow | General workflow-signature scan; exact temporary path check; legacy and obsolete root checks | Alternate, temporary, legacy, and obsolete-route tests | None |
| Explicit exclusions remain absent: updater/service, signing, release publication, prerequisite bootstrap, FoA deployment/game/save access, telemetry, proprietary toolkit, bundled personal data | Scoped installer implementation plus canonical workflow | `test_explicitly_excluded_installer_capabilities_remain_absent`; workflow forbidden-fragment validator | None in implementation |
| Retain unsigned development artifacts only | Canonical workflow final `actions/upload-artifact` step, 14-day retention | Pipeline-order test and workflow validator | A public release remains prohibited |

## Remaining evidence gates

### 1. Exact reviewed executable artifact

A compliant `FOA-SDK-Installer.exe` does not yet exist for the corrected PR head. The
following approved process must complete without substitution:

1. run canonical workflow `mode=inventory` on the exact final commit;
2. download and human-review the inventory, notices, and package inventory;
3. record the exact inventory SHA-256, named human reviewer, UTC review time, and durable evidence reference;
4. run canonical workflow `mode=package` on the same exact commit with those values;
5. require final artifact-set verification and install/manifest/shortcut/repair/uninstall smoke to pass;
6. retain the unsigned artifact and record its run, artifact ID, digests, exact source commit, review metadata, and lifecycle result in the durable PR #179 conversation without changing the reviewed source commit.

No automated actor may invent or impersonate step 2 or its review metadata.

### 2. Two-version major-upgrade evidence

The approved design explicitly leaves actual older-to-newer MSI smoke as a release
gate until two independently reviewed versions exist. Stable identity is implemented,
but upgrade execution must not be claimed from structure alone.

## Immutable completion evidence template

The exact reviewed source must not be changed merely to fill in post-build evidence:
doing so would create a new source commit and invalidate the reviewed inventory. After
the package run succeeds, the following completed record belongs in the durable PR #179
conversation, anchored to the exact reviewed source commit:

- Final source commit: **pending**
- Inventory workflow run: **pending**
- Reviewed inventory SHA-256: **pending human review**
- Reviewer / reviewed-at / evidence: **pending human review**
- Package workflow run: **pending**
- Unsigned artifact ID and digest: **pending**
- `FOA-SDK-Installer.exe` SHA-256: **pending**
- MSI SHA-256: **pending**
- ZIP SHA-256: **pending**
- Install / launcher / repair / uninstall result: **pending**
- Two-version upgrade smoke: **pending until two reviewed versions exist**
