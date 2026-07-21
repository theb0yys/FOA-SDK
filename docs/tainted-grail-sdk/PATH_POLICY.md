# Canonical Path and Executable Trust Policy

## Status

Accepted correction contract for Slice 2. This document changes no durable
schema and authorizes no runtime, deployment, or save behavior.

## One logical policy, two language bindings

The editor backend and Developer Preview tooling use one path-policy contract
across their C++ and Python language boundary:

- `PathPolicyService` is the authoritative editor persistence service;
- `developer_preview_path_policy.py` is the tooling binding for post-build shortcut creation and verification.

Both bindings must preserve the rules below. Neither UI widgets nor shortcut sidecar manifests are a root of trust.

## Canonical filesystem rules

1. Resolve absolute canonical paths before a security or write decision.
2. For a path that may not exist yet, resolve its existing ancestors and append only the unresolved tail.
3. Resolve a relative workspace root against the workspace document directory, never the process working directory.
4. Compare containment by path components, not string prefixes.
5. Apply case-insensitive component comparison on Windows and case-sensitive comparison elsewhere.
6. Resolve filesystem links before containment checks so a symlink or junction cannot escape an approved root.
7. Pack open, Save, and Save As must use a canonical `*.tgpack.json` path inside the canonical active workspace root.
8. Workspace open and save paths must use the `*.tgworkspace.json` suffix and are canonicalized before persistence.
9. UI code may suggest a path but must not create directories before the persistence boundary validates it. The boundary creates required parent directories only after canonical validation and revalidates the final path before writing.

## Executable trust modes

### Source-built verified entry

The standard clickable entry is trusted only when:

- the project and icon are derived from repository-owned paths and may not resolve through a symlink or junction outside the checkout;
- the build directory is exactly `build/tg-sdk-developer-preview-0-windows-profile` beneath the current repository;
- `CMakeCache.txt` contains an explicit `CMAKE_HOME_DIRECTORY` binding to the current repository source tree;
- the target is the expected `bin/profile/Editor.exe` or
  `bin/Profile/Editor.exe` beneath that approved build;
- the target has a valid x64 PE32+ Windows GUI executable header;
- the `.lnk` target, project argument, working directory, icon, description,
  size, and SHA-256 all match the repository-derived policy.

The sidecar records evidence about the generated shortcut. It does not define
the trusted target, project, icon, or working directory.

### Diagnostic override

An external `--editor` is allowed only with an explicit `--diagnostic-override` flag. It:

- is labeled `diagnostic-override` in its sidecar;
- must use a separate output beneath `build/diagnostic-entries`;
- cannot replace the standard verified entry;
- is rejected by normal verification;
- may be inspected only with an explicit diagnostic-verification flag;
- is never described as a verified source-built or release entry.

### Installed prebuilt entry

The standard installed entry is a separate trust mode from a source-built or
diagnostic entry. It is accepted only when:

- the launcher resides in the O3DE SDK `bin/Windows/profile/Default` install
  layout;
- the product root contains `INSTALL_MANIFEST.json`;
- the adjacent `Editor.exe` and installed `TaintedGrailModdingEditor/project.json`
  are regular files in that layout;
- the staged payload was bound to an exact reviewed inventory fingerprint and
  verified before MSI/ZIP assembly;
- MSI or ZIP provenance and published checksums identify the exact artifact.

The installed launcher never searches for another Editor, project, source tree,
game installation, or workspace. `--self-test` validates the installed layout
without launching the Editor. Installed-release trust does not make an unsigned
development artifact a signed or publicly approved release.

## Failure behavior

All violations fail closed with an actionable error. No path-policy failure
may fall back to a lexical prefix check, a manifest-provided expected value, an
arbitrary executable, or a direct persistence call.
