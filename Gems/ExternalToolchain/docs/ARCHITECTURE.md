# External Toolchain Architecture

## Role

`ExternalToolchain` is a host Tool Gem layered over O3DE's existing Gem activation model. It is not a second plugin loader. Provider Gems are ordinary O3DE Tool Gems and register typed descriptors with one host service.

## Implemented layers

### Provider contract

The public API owns stable provider and command identities, semantic provider versions, minimum host API compatibility, platform declarations, capability declarations, configuration keys, and discovery probes.

### Registration lifecycle

Provider registration is open during Editor system-component activation and closes at `OnPostActionManagerRegistrationHook`. The stable provider set is sorted by provider ID. Late or duplicate registration fails closed.

### Configuration

Providers declare configuration metadata but do not read arbitrary environment state through the host contract. Resolution is deterministic:

```text
session > user Settings Registry > project Settings Registry > provider default
```

The service retains the source layer, configured state, type-validity state, and sensitivity marker for every resolved value. Configuration is transient; this slice introduces no new durable document format.

### Discovery

After registration finalizes, the host evaluates provider probes in stable order. Discovery is limited to configured absolute local filesystem paths and `AZ::IO::SystemFile` file/directory inspection. It applies configured semantic-version bounds and returns typed candidates and provider-level results.

The discovery boundary rejects relative paths, parent traversal, URI paths, UNC/network paths, excessive provider/probe counts, invalid versions, wrong file/directory kinds, and incompatible platforms. Probe and provider elapsed times are diagnostic measurements. Time limits are best-effort post-call checks; the service does not claim it can interrupt a blocking operating-system filesystem call.

## Invariants

- O3DE Gems remain the packaging and activation mechanism.
- The host API is available only to host Tools and Builders variants.
- Provider and nested contract ordering is deterministic.
- Machine-specific paths belong in user or session configuration, not provider source defaults or shared project files.
- Sensitive values have no provider defaults and are masked in the Editor pane.
- Discovery success means only that a configured local path exists, has the declared kind, and satisfies configured version bounds.
- Discovery success does not authorize execution.
- Discovery has no process or file-generation authority. M2 execution is a separate, default-disabled service; shell, installation, asset promotion and game/deployment operations remain outside it.

## Data flow

```text
Provider Tool Gems activate
    -> register typed descriptors
    -> host finalizes stable provider set
    -> resolve project/user/session configuration
    -> inspect bounded local path candidates
    -> publish read-only discovery diagnostics
```

## Ordered follow-on work

1. M2 host-owned process supervision under the [accepted Windows LPAC decision](../../../docs/tainted-grail-sdk/TOOL_EXECUTION_M2_DESIGN.md), implemented with explicit admission. Its private backend supersedes the earlier suggested ProcessWatcher route because containment must fail closed.
2. Descriptor-generated Action Manager commands and parameter forms.
3. Source-artifact manifests and Asset Processor handoff.
4. Independent Blender and heightmap reference Provider Gems.
5. File-backed Unity interchange, provenance, and third-party qualification.
6. Structured IPC, hot-loading, signing, and third-party trust research gates.

Each follow-on slice requires separate design review and tests. Discovery results are not execution permission.

The FoA-specific provider inventory, Blender qualification, interchange contract, loss model, and ordered
delivery gates are defined in the
[FoA editor-toolchain design](../../../docs/tainted-grail-sdk/EDITOR_TOOLCHAIN_UNITY_INTERCHANGE_DESIGN.md).

## M2 execution ownership

The execution service consumes registered batch commands, a trusted host discovery
snapshot and exact admission. Public request records contain stable root IDs and
relative file references. Only the host resolves absolute paths; these paths do
not enter shared invocation records.

Two joined workers serve a queue of at most sixteen attempts. The API mutex does
not cover filesystem I/O, process waits or loading saved logs. A serialised
transition closes the cancellation window when draining starts. There are no
worker callbacks into Editor components.

The Windows backend validates local path components and holds filesystem
identities, creates read-only snapshots, starts suspended with an explicit handle
allowlist and exactly the enabled registryRead capability SID, verifies effective LPAC restrictions, assigns
a kill-on-close job, rechecks admission and then resumes. Output manifests are
checked only after every job process stops. The discovery Core remains process-free.

The host-private journal uses an exclusive writer and two flushed record slots.
Recovery inspects only recorded staging/profile resources, retains ambiguous
leftovers and records interruption without replay. Named kernel locks currently
provide conservative provider/root exclusion across hosts; including ancestor
identities serialises targets on the same volume. This implementation detail and
the remaining profile compatibility failures are recorded in the M2 decision.

M2 does not mint Foundation permission, write M1 phase receipts or promote outputs
into packs. Editor deactivation disconnects and joins the service. There is no M2
launch button or provider-specific UI.

The pinned Windows Editor terminates from its early exit sequence before normal
component deactivation. The host therefore joins the supervisor synchronously
from Qt `aboutToQuit`, connected through `NotifyQtApplicationAvailable`, and also
from `Deactivate`. The stored Qt connection is disconnected on deactivation;
shutdown is idempotent. A real Editor trace must observe the early joined-shutdown
marker and process exit, rather than assuming that a component destructor runs.
