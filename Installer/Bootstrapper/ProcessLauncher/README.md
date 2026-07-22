# Capability-gated process launcher

`ProcessLauncher/` is the installer component permitted to start one reviewed helper process.

It consumes an exact package-engine session plus a separate short-lived grant whose capability is:

```text
package-engine.launch-process
```

The grant binds:

- the exact `session_sha256`;
- a logical executable reference;
- the executable SHA-256;
- the complete argument vector and `argv_sha256`;
- the complete replacement environment and `environment_sha256`;
- timeout and captured-output limits;
- issuer, nonce, issue time, expiry time and `grant_sha256`.

Machine paths are not serialized into the grant. At execution time the caller supplies an absolute executable path and working directory. The launcher rejects missing, relative or symbolic-link paths, re-hashes the executable, and refuses any mismatch.

Process creation uses an exact argument array with `shell=False`, closed standard input, captured output, a fully replaced environment, bounded timeout, bounded output and closed inherited file descriptors. There is no PATH lookup, shell parsing, command concatenation, elevation verb, `sudo`, `runas`, `ShellExecute`, MSI invocation, network access or game launch.

The result records the executable, argv and environment fingerprints, return code or timeout state, bounded stdout/stderr hashes and sizes, and explicit `process_launched=true`, `shell_used=false`, and `elevation_requested=false` values. The result grants no later authority and does not finalize install, repair, upgrade, rollback or uninstall.

The elevation helper and lifecycle finalizers remain separate installer gates.
