# FOA Mono Runtime Adapter

This optional package is the project-owned Mono runtime-adapter source package for the exact pinned profile:

```text
profile: foa-1.23.401-mono-bepinex5-tf-0.1.33
game: 1.23.401
branch: mono
runtime: Mono
Unity: 6000.0.64f1
BepInEx: 5.4.23.3
Tainted Framework: 0.1.33
evidence state: HostLiveLoadValidated
```

The package is deliberately separate from the future IL2CPP adapter. It shares no target-specific source, binaries, dependencies, compatibility declarations, execution requests, or runtime-result evidence with IL2CPP.

## Contents

- `plugin.json` — optional FOA-SDK package registration.
- `mono-adapter.json` — exact profile, dependencies, source bindings, expected binaries, evidence schemas, and all-false authority record.
- `Source/` — project-owned BepInEx 5 source only.
- `Schemas/execution-gate.schema.json` — review-readiness envelope for a separately reviewed external executor.
- `Schemas/runtime-result.schema.json` — externally supplied runtime-result evidence.
- `Tools/mono_runtime_adapter.py` — deterministic package verification, non-executable build planning, gate evaluation, and result verification.
- `Tests/` — focused package and mutation coverage.

## Build boundary

`plan-build` emits the exact reviewed `dotnet build` argument vector, source fingerprints, local dependencies, and expected outputs. The plan permanently records `execution_allowed=false` and every authority field false.

The package does not invoke `dotnet`, inspect an operator's game installation, download dependencies, copy binaries, deploy files, launch the game, patch gameplay, mutate saves, sign, or publish.

The generated assembly is expected at:

```text
bin/Release/netstandard2.1/FOA.SDK.RuntimeAdapter.Mono.dll
```

Generated DLLs and PDBs belong beneath an external build root and must not be committed here.

## External execution gate

`evaluate-gate` binds:

- the exact package build-plan fingerprint;
- one externally built Mono DLL fingerprint and byte count;
- the exact Mono profile;
- distinct active adapter-identity and install-parity evidence IDs;
- one verified Suite Wizard confirmation-receipt fingerprint;
- named requester and whole-second UTC execution window.

A successfully constructed gate means only `ready_for_external_executor_review=true`; `execution_authorized` remains false. It grants no process, game-launch, deployment, runtime, mutation, save, signing, publication, catalog, or evidence-promotion authority. A later executor must be separately reviewed and must reverify the complete gate, the named evidence, the confirmation receipt, and the binary.

## Runtime evidence

The runtime-result contract records:

- exact gate and binary fingerprints;
- typed outcome;
- chronology;
- loader, Tainted Framework, and startup-marker observations;
- a safe relative logical log reference;
- bounded failures;
- candidate-evidence eligibility;
- all-false surviving authority.

The verifier never reads a game log or promotes evidence. `candidate_evidence_eligible=true` is not publication or catalog authority.

## Commands

```powershell
python Plugins/RuntimeAdapters/Mono/Tools/mono_runtime_adapter.py verify-package

python Plugins/RuntimeAdapters/Mono/Tools/mono_runtime_adapter.py plan-build

python Plugins/RuntimeAdapters/Mono/Tools/mono_runtime_adapter.py evaluate-gate `
  --binary-sha256 sha256:<64 hex> `
  --binary-bytes <positive integer> `
  --identity-evidence-id evidence.adapter.mono.identity `
  --parity-evidence-id evidence.adapter.mono.parity `
  --confirmation-receipt-sha256 sha256:<64 hex> `
  --requested-by operator.foa `
  --requested-at-utc 2026-07-22T06:00:00Z `
  --expires-at-utc 2026-07-22T07:00:00Z
```

No command in this package launches a process or writes into the game installation.
