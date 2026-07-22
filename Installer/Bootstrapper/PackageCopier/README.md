# Package payload copier

`PackageCopier/` is the first installer component permitted to mutate the filesystem.

It consumes one exact package-engine session plus a separate short-lived copy grant bound to that session's `session_sha256`. The grant capability is exactly:

```text
package-engine.copy-payload
```

The copier reads the resolver-owned payload inventory embedded in the session, verifies every source file's SHA-256 and byte count, writes every destination beneath a fresh temporary staging directory, verifies every staged file, and publishes the completed staging tree with one final directory rename.

The staging root must not already exist. Its parent and the entire source path must be non-symlink paths. Destination paths remain relative, case-collision-free, and resolver-owned. A failure removes the temporary staging tree and publishes no destination directory.

This unit does not copy into a product or game installation directory. It does not launch a process, request elevation, finalize install/repair/upgrade/rollback/uninstall, execute runtime adapters, deploy content, mutate saves, sign, upload, publish, mutate catalogues, or promote evidence.

The next isolated gates are the process launcher and elevation helper. Lifecycle finalization remains separate from both.
