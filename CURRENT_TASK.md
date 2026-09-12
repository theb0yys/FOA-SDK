# M2 - Tool Execution Service

Status: IMPLEMENTED; submit for maintainer review with exact-head validation.
Goal: ExternalToolchain supervisor, V2 API, cancellation, bounded diagnostics, verified outputs and durable records.
Classification: Critical/Runtime. Primary owner: external-toolchain.
Authority: original M2 scope and amendment B accepted by the owner on 12 September 2026; commit and PR explicitly requested.
In scope: the exact 43 paths in TOOL_EXECUTION_M2_DESIGN.md; Windows LPAC registryRead batch profile and synthetic proof.
Implemented: V2 contracts, admission and bounded workers, LPAC/Job Object backend, output validation, private journal/recovery, native fixture and Editor lifecycle integration. Editor execution remains default-disabled pending separate Framework integration.
Compatibility: windows-lpac-registry-read-batch-v1 requires exactly one enabled registryRead SID. Old profile commands and their request fingerprints refuse. Historical records do not grant replay authority.
Validation: final production-source results belong to the exact-head private receipt and PR. Earlier amendment A experimental results remain separate evidence.
Current branch: codex/tool-execution-m2-design, isolated from concurrent SDK-client work.
Next action: maintainer review of the M2 PR. M3 requires a new owner instruction.
Out of scope: interactive tools, secrets, game/Unity launch, deployment, release and protected data.
Runtime sign-off not performed.
