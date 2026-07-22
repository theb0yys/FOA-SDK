# Capability-gated elevation helper

`ElevationHelper/` is the isolated operating-system elevation boundary for the FOA-SDK installer.

It requires all of the following before an elevation prompt may be requested:

1. an exact verified package-engine session containing `package-engine.elevation`;
2. an exact verified process-launch grant bound to that session;
3. an explicit authenticated user-consent record bound to the launch grant and helper identity;
4. a separate elevation grant issued after consent and valid for at most five minutes;
5. a reviewed controlled-bootstrapper executable and support-file inventory whose hashes and byte counts match the signed operation plan;
6. a canonical create-once bootstrap request bound to the exact helper, bootstrapper, grant, consent, session, and request time.

The source bootstrapper path is never passed directly to UAC after hashing. The helper resolves and verifies the reviewed executable plus every declared support file, copies that complete bundle beneath a private create-once directory, verifies the copy, and passes the private executable path to `ShellExecuteW` with the fixed `runas` verb. This closes the mutable-path interval between verification and elevation launch.

All deterministic path, hash, bundle, request-publication, chronology, and consent checks complete before the one-shot elevation grant is consumed. The production Windows backend does not accept or capture credentials, cannot suppress the operating-system consent UI, does not use a shell command string, and does not inherit new installer authority.

The grant turns on only the existing `elevation` authority field. Acquisition, installation finalization, game launch, runtime execution, deployment, save mutation, signing, publication, catalogue mutation, and evidence promotion remain false.

The immediate elevation result records only whether the operating system accepted the private-bootstrapper request. It deliberately sets `process_completion_observed = false`; lifecycle success must be established by the separately authenticated controlled-bootstrapper completion receipt and elevated-completion observation gate.

The controlled bootstrapper revalidates canonical request bytes, helper policy, support-file hashes, paths, and timing. It executes a verified private helper bundle with one combined output limit and publishes its completion receipt with full-write, fsync, canonical validation, atomic no-replace publication, and exact readback verification. A raw partial `os.write` is not an accepted completion-publication path.

Hosted tests use an injected fake UAC backend and never display a real prompt. They require the backend to receive a private hash-identical bootstrapper path rather than the mutable source path, exercise support-file copying, verify canonical request and completion publication, and prove replay rejection.
