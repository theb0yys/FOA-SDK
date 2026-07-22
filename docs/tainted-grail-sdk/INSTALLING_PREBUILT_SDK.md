# Installing the Prebuilt Windows SDK

## Current availability

The prebuilt installer is a development-channel capability and is not yet a
supported public release. Use only an artifact supplied from an exact reviewed
repository commit. Current artifacts are unsigned, so Windows may display an
unrecognized-app warning. Do not treat that warning as proof of either safety or
malice; verify the supplied checksums and provenance before deciding to run it.

## Requirements

- Windows 64-bit;
- sufficient space for the prebuilt O3DE Editor and SDK;
- a writable location for TG SDK workspaces and generated data;
- no Git, Python, CMake, Visual Studio, or O3DE source build.

A legitimate FoA installation is needed only when you configure a real game
profile. The installer does not discover, modify, launch, or deploy to FoA.

## Verify the artifact

The standard artifact is `FOA-SDK-Installer.exe`. Compare its SHA-256 with
`FOA-SDK-Installer.exe.sha256` before running it. The checksum verifies the
downloaded bytes against the reviewed artifact metadata; current development
artifacts remain unsigned, so it is not a publisher signature.

For a portable ZIP, compare its SHA-256 value with the adjacent `.sha256` file.
After extraction, retain these files at the product root:

- `INSTALL_MANIFEST.json` — exact product, commit, review, and file inventory;
- `SHA256SUMS` — checksums for every manifest payload file and the manifest;
- `BUILD_PROVENANCE.json` — source and inventory binding;
- `SBOM.spdx.json` — SPDX 2.3 file inventory;
- `NOTICES.txt` and `THIRD_PARTY_PACKAGES.json` — redistribution-review inputs.

Missing or mismatched metadata is a failed artifact. Do not substitute files
from another build or copy raw files into the installation.

## Executable wizard installation

1. Close any running O3DE Editor and Asset Processor instances.
2. Run the reviewed `FOA-SDK-Installer.exe`.
3. Select **Install or upgrade the complete FOA-SDK**.
4. Choose the default per-user location or another absolute writable location.
5. Review the displayed MSI SHA-256 and external-workspace boundary, then select
   **Apply**.
6. Leave **Launch Tainted Grail Modding Editor** selected, or open it later from
   the **Tainted Grail FoA SDK** Start
   Menu folder.

The executable contains the reviewed MSI and verifies its captured bytes before
starting Windows Installer. No Python, Git, CMake, Visual Studio, source checkout,
or separately installed .NET runtime is required.

The launcher checks the installed manifest, Editor, and dedicated project before
starting `Editor.exe --project-path <installed-project>`. An actionable error
asks you to repair or reinstall when that layout is incomplete.

Keep real workspaces, imported evidence, generated output, staging, deployment,
and FoA diagnostic data outside the installation directory. Those external
locations are not installer-owned and must survive repair, upgrade, and
uninstall.

## Repair, upgrade, and uninstall

Run the same reviewed executable and select **Repair** or **Uninstall**. Windows
**Settings → Apps → Installed apps → Tainted Grail FoA Modding SDK** remains the
standard fallback for uninstall. A newer reviewed executable containing an MSI
with the same Upgrade Code and a newer version replaces the older product. Do
not rename a rebuild to impersonate a new version or install an older version
over a newer one.

Repair restores product-owned files from the exact MSI. It does not validate or
repair external workspaces. Uninstall removes product-owned files and leaves
external workspace data untouched.

## Direct MSI recovery

The reviewed MSI is retained as a separate development artifact for Windows
Installer recovery and operator diagnosis. Running it directly provides the
same product payload and lifecycle registration, but the standard user path is
the executable wizard. Never pair the executable with an MSI or checksum from a
different reviewed build.

## Portable ZIP

Extract the ZIP to a new empty directory; do not overlay another version. Run:

```text
bin\Windows\profile\Default\TaintedGrailModdingEditorLauncher.exe
```

The ZIP and MSI contain the same staged manifest payload. The ZIP has no Windows
Installer repair or uninstall registration; remove its extraction directory
manually only after confirming that no workspace was placed inside it.

## Troubleshooting

- **Executable checksum mismatch:** stop and obtain a new reviewed artifact.
- **Manifest missing:** run the executable's Repair operation or obtain the complete artifact.
- **Editor/project missing:** run Repair; do not point the launcher at an
  arbitrary `Editor.exe`.
- **Checksum mismatch:** stop and obtain a new reviewed artifact.
- **Editor opens without TG SDK panes:** record the exact artifact version,
  source commit, launcher error/log, and complete manual Editor UI checklist.
- **Upgrade refused:** retain the installer log, uninstall the conflicting development build, preserve
  external workspaces, then install the reviewed replacement.

See [Windows Installer and Prebuilt Artifact Workflow Design](WINDOWS_INSTALLER_AND_ARTIFACT_WORKFLOW_DESIGN.md)
for the build, review, trust, and non-release boundary.
