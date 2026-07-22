# Package payload copier

`PackageCopier/` is the first installer component permitted to publish a staged payload tree.

It consumes one exact package-engine session plus a separate short-lived copy grant bound to that session's `session_sha256`. The grant capability is exactly:

```text
package-engine.copy-payload
```

The copier reads the resolver-owned payload inventory embedded in the session, verifies every source file's SHA-256 and byte count, writes every destination beneath a fresh temporary staging directory, and verifies every staged file.

Source-root, staging-root, parent-containment, symlink, inventory, collision, source-byte, and staged-byte checks all complete before the one-shot grant is consumed. A preflight failure removes the temporary tree and leaves the grant available for a corrected attempt. After successful preflight, the grant claim is created immediately before atomic no-replace publication of the completed staging directory.

The staging root must be an absent absolute path. Its parent and the entire source path must be non-symlink paths. Destination paths remain relative, case-collision-free, and resolver-owned. An existing destination is never overwritten or merged.

This unit does not copy into a product or game installation directory. It does not launch a process, request elevation, finalize install/repair/upgrade/rollback/uninstall, execute runtime adapters, deploy content, mutate saves, sign, upload, publish to a network, mutate catalogues, or promote evidence.

The next isolated gates are the process launcher and elevation helper. Lifecycle finalization remains separate from both.
