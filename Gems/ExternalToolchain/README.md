# External Toolchain Host

`ExternalToolchain` is an O3DE host-tools Gem that supplies a common, versioned boundary for independently packaged external editor-tool providers.

The current implementation includes:

- a public `Gem::ExternalToolchain.API` target;
- deterministic provider, command, configuration, and discovery-probe registration;
- registration finalization after Action Manager registration;
- layered configuration with `session > user > project > provider default` precedence;
- bounded, deterministic inspection of configured local files and directories;
- configured Semantic Version compatibility checks;
- typed discovery statuses and read-only diagnostics;
- focused C++ contract tests and repository validation.

It intentionally does **not** launch external processes, execute shell commands, inspect remote/network paths, install applications, perform IPC, generate files, write the Asset Processor cache, hand off assets, hot-load providers, or add runtime/game behavior.

## Provider registration

Provider Gems depend on `Gem::ExternalToolchain.API`, require the `ExternalToolchainService`, and register during their Editor system component activation:

```cpp
ExternalToolchain::ExternalToolProviderDescriptor provider;
provider.m_providerId = "example.heightmap";
provider.m_displayName = "Example Heightmap Generator";
provider.m_providerVersion = "1.0.0";
provider.m_minimumHostApiVersion = { 1, 1, 0 };
provider.m_toolFamily = ExternalToolchain::ToolFamily::Generator;
provider.m_platforms = { "windows", "linux" };
provider.m_capabilities.m_supportsBatch = true;
provider.m_capabilities.m_supportsHeadless = true;
provider.m_capabilities.m_producesAssetSources = true;

ExternalToolchain::ExternalToolCommandDescriptor command;
command.m_commandId = "generate-heightmap";
command.m_displayName = "Generate Heightmap";
command.m_mode = ExternalToolchain::CommandMode::Batch;
command.m_outputKinds = { "image/heightmap" };
provider.m_commands.push_back(command);

ExternalToolchain::ExternalToolConfigurationDescriptor executablePath;
executablePath.m_key = "executable-path";
executablePath.m_displayName = "Executable path";
executablePath.m_kind = ExternalToolchain::ConfigurationValueKind::Path;
executablePath.m_required = true;
provider.m_configuration.push_back(executablePath);

ExternalToolchain::ExternalToolConfigurationDescriptor toolVersion;
toolVersion.m_key = "tool-version";
toolVersion.m_displayName = "Tool version";
toolVersion.m_kind =
    ExternalToolchain::ConfigurationValueKind::SemanticVersion;
provider.m_configuration.push_back(toolVersion);

ExternalToolchain::ExternalToolDiscoveryProbeDescriptor probe;
probe.m_probeId = "configured-executable";
probe.m_kind = ExternalToolchain::DiscoveryProbeKind::File;
probe.m_pathConfigurationKey = "executable-path";
probe.m_versionConfigurationKey = "tool-version";
probe.m_minimumSupportedVersion = "1.0.0";
probe.m_platforms = { "windows", "linux" };
provider.m_discoveryProbes.push_back(probe);

ExternalToolchain::ProviderOperationResult result;
ExternalToolchain::ExternalToolchainRequestBus::BroadcastResult(
    result,
    &ExternalToolchain::ExternalToolchainRequests::RegisterProvider,
    provider);
```

Registration closes during `OnPostActionManagerRegistrationHook`. Late registration fails closed. Provider changes require an Editor restart in this milestone.

## Configuration layers

Providers declare keys and optional defaults. The host resolves each key in this order:

1. session overrides supplied through the host request API;
2. `/O3DE/ExternalToolchain/Configuration/User/Providers/<provider-id>/<key>`;
3. `/O3DE/ExternalToolchain/Configuration/Project/Providers/<provider-id>/<key>`;
4. the provider descriptor default.

`Enabled` is a reserved host-owned boolean below each provider object and follows the same layer order. Empty higher-layer strings intentionally clear lower-layer defaults. Sensitive resolved values are masked in the diagnostics pane.

See [Discovery and Configuration](docs/DISCOVERY_AND_CONFIGURATION.md) for status semantics, path restrictions, Settings Registry examples, and testing boundaries.

## Build boundary

The Gem is host-tools-only. It creates `Tools` and `Builders` aliases but no Client, Server, or Unified runtime aliases.

See [Architecture](docs/ARCHITECTURE.md) for the researched delivery sequence and boundary rules.
