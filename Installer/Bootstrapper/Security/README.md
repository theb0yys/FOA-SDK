# Installer execution security boundary

`Bootstrapper/Security/` is the common authority and effect-safety boundary for the operational installer chain.

It owns:

- reviewed operation plans bound to the exact resolver `plan_sha256` and `package_order`;
- package-owned helper allow-lists with exact executable path, digest, reviewed support-file inventory, arguments, working directory, environment, timeout, and one combined output bound;
- an external installer-authority trust key used to authenticate authority proofs and every executable grant/result;
- create-once one-shot claim records for tokens, copy grants, launch grants, elevation grants, bootstrap requests, and state-publication grants;
- atomic no-replace file and directory publication;
- bounded strict UTF-8 JSON loading with canonical-byte, symlink, size, depth, and node-count checks;
- symlink-safe exact path resolution and SHA-256 verification;
- verified private helper-bundle copies rather than mutable source-path execution;
- bounded streaming stdout/stderr capture under one shared byte budget with process-tree termination;
- absolute System32 process-tree cleanup on Windows rather than ambient `PATH` resolution.

## Trust anchor

The authority key is raw 32–64 byte key material owned by the trusted installer broker. It must be stored outside package payloads, source trees, logs, receipts, and command output. On POSIX hosts, group and other permissions are rejected. Production Windows packaging must provision the key with an ACL readable only by the broker identity.

Possession of a token, session, grant, or receipt is not sufficient to execute an effect. The effect boundary also requires the configured authority key. Caller-controlled paths, timestamps, helper bytes, support files, destinations, and deterministic preconditions are validated before the one-shot authority claim is consumed; the claim remains immediately adjacent to the authorized publication or process transition.

## Reviewed helper ownership

Every helper is owned by one package in the exact resolver package order. An operation plan is rejected unless its resolver plan fingerprint and package order match the receipt handoff authenticated by the authority proof.

Callers select a helper reference only. They cannot supply executable paths, support-file paths or hashes, arguments, environment variables, working directories, timeout, or output limits at launch time.

A helper that needs adjacent DLLs, runtime configuration, native libraries, scripts, or data must declare each support file with its exact relative path, SHA-256, and byte count. Launch copies and verifies the complete declared bundle beneath a private create-once directory. Undeclared sidecars are not carried into the private execution bundle.

## Non-authority

These primitives grant no network publication, runtime adapter execution, game launch, save mutation, signing, catalogue mutation, or evidence promotion authority.
