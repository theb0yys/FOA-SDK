# Mod Package Builder

Open **Mods** in the Development Hub, save your mod, and choose **Build and export package**.

## Preview and export

Choose **Preview current mod**. The file inventory contains the selected mod, its declared workspace pack dependencies, owned authoring definitions, translations, assignments and reviewed managed images. Native game identities are retained as unverified references; imported game definitions and extracted assets are excluded.

Resolve any reported missing dependency, conflicting pack, broken reference, unsupported manifest declaration, missing image or redistribution review. Image redistribution is declared in **Assets and text**. Legacy actor portraits need a managed Asset Manager entry with reviewed redistribution. Save changes and preview again.

Choose **Export package** and a new .tgmod filename outside the workspace. Export verifies the preview's current inputs, writes to a temporary file, verifies its bytes and publishes without replacing an existing file. The result is an editable FOA authoring archive, not a runtime mod. The format is deterministic JSON with base64 entry bytes, a manifest and SHA-256 checksums; no compression or external tool is needed.

## Inspect and reopen

Choose a package and press **Inspect package**. Select the matching locally configured game profile, enter a new workspace folder under an existing parent, then choose **Create workspace**. Existing folders are rejected. The service verifies all entries, reconstructs the workspace in temporary staging and publishes only the completed result.

Press **Open created workspace** when ready to switch. Other authoring panes retain their existing draft protection. The imported pack keeps its content IDs, definitions, images, translated text and assignments. Its local game paths come from your current configuration. Diagnostics and extraction folders are created inside the new workspace. Catalog permission and validation history are excluded; review imported content before any later use.

## Scope and limits

The archive supports current catalog schema 7 and pack schema 1. It accepts at most 512 local packs, 4096 files, 64 MiB of decoded content and a 96 MiB archive. PNG/JPEG limits remain those of the Asset Manager. Unknown versions, unknown fields, unsafe/colliding paths, corrupt entries and mismatched profiles are rejected.

Dependencies are exact local pack IDs, with the versions included in their manifests. Runtime mod requirements are declarations only. Native definition edits without durable pack ownership are not inferred into the selected mod. Arbitrary manifest files, mesh/audio conversion, runtime building, installation, deployment, saves, signing and publication are separate capabilities.

Cancel stops the current operation and preserves existing files. An unsuccessful import removes only its own temporary staging folder. Checksums detect corruption; they do not prove authorship, licensing or runtime compatibility.

## Validation

The compiled ModPackageExportTests exercise real export and clean-workspace reopening, mixed authoring definitions, images/text/assignments, dependency closure, corruption, cancellation, immutable outputs and failure recovery. The Editor smoke under Tools/editor_tests exercises the actual buttons and verifies background responsiveness. Validation results belong to the exact tested source and are recorded in the PR.


To reproduce the synthetic Editor test, run the package fixture builder with a new output directory:

```text
python Gems/TaintedGrailModdingSDK/Tools/editor_tests/mod_package_fixture.py --output <new-fixture-directory>
```

Set FOA_SDK_ASSET_WORKSPACE to its preview.tgworkspace.json and FOA_SDK_PACKAGE_RESULT to a new result JSON path in a separate empty evidence directory. Launch the matching built Editor with an isolated project and the processed host assets:

```text
Editor --project-path <isolated-project> --engine-path <pinned-engine> --skipWelcomeScreenDialog --runpython <source>/Gems/TaintedGrailModdingSDK/Tools/editor_tests/mod_package_live_smoke.py
```

The test creates original synthetic artwork and package/workspace outputs under the evidence directory. Use a new fixture and evidence directory on each run; existing archives and destinations are intentionally not replaced.


Pinned-host UI test note: do not use --autotest_mode for these dialog tests. The current host also treats the substrings -export and /export anywhere in its complete launch command as unattended mode and closes modal dialogs. If the source or build path contains those substrings, copy both smoke scripts into the private evidence directory (verify their hashes) and use an explicit executable path with the child argv[0] set to Editor.exe. The tested launch adds --exec_line "ed_keepEditorActive 1" so the event loop progresses while the Editor is in the background. No engine-source change is required.
