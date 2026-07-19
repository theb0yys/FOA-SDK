# Provider Discovery and Configuration

## Scope

This contract determines which registered external-tool providers are enabled, which configured local installation paths exist, and whether caller-configured tool versions fall inside provider-declared compatibility bounds.

It does not launch or interrogate applications. Version values are configuration inputs; they are not extracted by running an executable.

## Settings Registry layout

Shared project configuration:

```json
{
    "O3DE": {
        "ExternalToolchain": {
            "Configuration": {
                "Project": {
                    "Providers": {
                        "example.heightmap": {
                            "Enabled": true,
                            "executable-path": "C:/StudioTools/Heightmap/heightmap.exe",
                            "tool-version": "2.4.0"
                        }
                    }
                }
            }
        }
    }
}
```

Machine-specific configuration uses the equivalent `User/Providers` root. Session overrides are in-memory only and disappear when the Editor closes.

Precedence is exact:

```text
session > user > project > provider default
```

An empty higher-layer string clears a lower-layer default. Required empty values are invalid. Semantic-version values must use SemVer. Sensitive values cannot have provider defaults and are masked in the diagnostics pane.

## Discovery statuses

- `not_run` — no applicable probe exists or discovery has not executed;
- `disabled` — host or provider configuration disables discovery;
- `unsupported_platform` — the provider does not support the current host platform;
- `not_installed` — configured paths were inspected and do not exist;
- `installed` — exactly one compatible configured local path exists;
- `unsupported_version` — the path exists but the configured version is outside provider bounds;
- `misconfigured` — required data, value type, path form, or file/directory kind is invalid;
- `probe_failed` — configured provider/probe limits or best-effort elapsed-time limits were exceeded;
- `ambiguous` — more than one distinct compatible configured path exists.

Results and candidates are sorted by stable provider/probe identity. Normalized duplicate Windows paths are inspected once.

## Path boundary

Discovery accepts only absolute local paths. It rejects:

- relative paths;
- `..` traversal segments;
- URI-style paths;
- UNC and other network paths;
- values above the configured length limit;
- a directory where a file was declared, or a file where a directory was declared.

The service uses local filesystem metadata only. It performs no recursive scan and does not search an entire disk, PATH, registry hive, package manager, network share, or application database.

## Bounded-operation note

Provider count, probes per provider, individual probe time, and provider elapsed time have configured limits. File inspection is synchronous and measured. A slow call is reported after it returns; this API does not claim to pre-empt or cancel an operating-system filesystem call. Network paths are therefore prohibited.

## Security and authority

An `installed` result is read-only diagnostic evidence. It does not prove application authenticity, publisher identity, binary integrity, licence entitlement, safe execution, or correct version metadata. It never grants process-launch, shell, IPC, asset, deployment, or runtime permission.
