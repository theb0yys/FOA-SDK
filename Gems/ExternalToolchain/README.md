# External Toolchain Host

`ExternalToolchain` is an O3DE host-tools Gem that defines the common provider-registration boundary for independently packaged external editor tools.

The foundation slice implements:

- a public `Gem::ExternalToolchain.API` target;
- versioned provider and command descriptors;
- deterministic provider validation and registration;
- explicit registration finalization after Action Manager registration;
- an Editor diagnostics pane showing registered providers;
- focused C++ contract tests and a repository validator.

It intentionally does **not** implement external process launch, tool discovery, IPC, file generation, Asset Processor handoff, hot loading, Blender integration, Unity integration, or runtime/game behavior.

## Provider registration

Provider Gems depend on `Gem::ExternalToolchain.API`, require the `ExternalToolchainService`, and register during their Editor system component activation:

```cpp
ExternalToolchain::ExternalToolProviderDescriptor provider;
provider.m_providerId = "example.heightmap";
provider.m_displayName = "Example Heightmap Generator";
provider.m_providerVersion = "1.0.0";
provider.m_minimumHostApiVersion = ExternalToolchain::HostApiVersion;
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

ExternalToolchain::ProviderOperationResult result;
ExternalToolchain::ExternalToolchainRequestBus::BroadcastResult(
    result,
    &ExternalToolchain::ExternalToolchainRequests::RegisterProvider,
    provider);
```

Registration closes during `OnPostActionManagerRegistrationHook`. Late registration fails closed. Provider changes require an Editor restart in this milestone.

## Build boundary

The Gem is host-tools-only. It creates `Tools` and `Builders` aliases but no Client, Server, or Unified runtime aliases.

See [Architecture](docs/ARCHITECTURE.md) for the researched delivery sequence and boundary rules.
