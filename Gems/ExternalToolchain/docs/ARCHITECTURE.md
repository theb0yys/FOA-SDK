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
- No shell, process, IPC, download, install, file generation, asset handoff, cache mutation, runtime adapter, deployment, or game launch exists in this slice.

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

1. Host-owned process supervision using argument vectors and `AzFramework::ProcessWatcher`.
2. Descriptor-generated Action Manager commands and parameter forms.
3. Source-artifact manifests and Asset Processor handoff.
4. Independent Blender and heightmap reference Provider Gems.
5. Unity interchange, structured IPC, hot-loading, provenance, signing, and third-party trust research gates.

Each follow-on slice requires separate design review and tests. Discovery results are not execution permission.
