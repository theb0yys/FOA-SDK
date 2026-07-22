# Capability-gated process launcher

`ProcessLauncher/` is the installer component permitted to start one reviewed helper process.

It consumes an exact package-engine session plus a separate short-lived grant whose capability is:

```text
package-engine.launch-process
```

The grant binds:

- the exact `session_sha256`;
- a reviewed logical helper reference;
- the executable SHA-256;
- the sorted reviewed support-file inventory and fingerprint;
- the complete argument vector and `argv_sha256`;
- the complete replacement environment and `environment_sha256`;
- the reviewed working-directory reference;
- timeout and one combined stdout/stderr byte limit;
- issuer, nonce, issue time, expiry time and `grant_sha256`.

Machine paths are not serialized into the grant. At execution time the caller supplies only the trusted execution root and claim root. The launcher resolves the reviewed relative paths, rejects missing or symbolic-link paths, re-hashes the executable and every declared support file, and copies the complete verified bundle beneath a private create-once directory. The process is launched from those private executable bytes, not from the mutable source path.

All deterministic path, hash, bundle, chronology, and elevation-policy checks happen before the one-shot launch grant is consumed. A preflight failure removes its private bundle and leaves the grant unconsumed. After a normal launch attempt, the private bundle is removed and the result records that cleanup.

Process creation uses an exact argument array with `shell=False`, closed standard input, captured output, a fully replaced environment, bounded timeout, one combined output budget and closed inherited file descriptors. There is no PATH lookup for the reviewed helper, shell parsing, command concatenation, elevation verb, `sudo`, `runas`, `ShellExecute`, MSI invocation, network access or game launch. Windows process-tree cleanup resolves `taskkill.exe` from System32 rather than the ambient environment.

The result records executable, support-file, argv and environment fingerprints, return code or timeout state, combined output accounting, bounded stdout/stderr hashes and sizes, private-bundle cleanup, and explicit `process_launched=true`, `shell_used=false`, and `elevation_requested=false` values. The result grants no later authority and does not finalize install, repair, upgrade, rollback or uninstall.

The elevation helper and lifecycle finalizers remain separate installer gates.
