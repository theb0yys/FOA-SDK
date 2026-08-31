// SPDX-License-Identifier: Apache-2.0 OR MIT
using System.Reflection;
using System.Text.Json;

namespace FOA.SDK.ControlPanel;

internal sealed record ProviderManifest(
    string ProviderId,
    string DisplayName,
    IReadOnlyList<string> Capabilities,
    IReadOnlyList<string> RuntimeRoutes);

internal static class ProviderManifestLoader
{
    private const string ResourceName = "FOA.SDK.ControlPanel.Providers.foa.provider.json";
    private const int MaximumManifestBytes = 256 * 1024;

    public static ProviderManifest LoadBundledFoaProvider()
    {
        Assembly assembly = Assembly.GetExecutingAssembly();
        using Stream stream = assembly.GetManifestResourceStream(ResourceName)
            ?? throw new InvalidOperationException("The bundled Fall of Avalon provider manifest is missing.");
        if (stream.Length <= 0 || stream.Length > MaximumManifestBytes)
        {
            throw new InvalidOperationException("The bundled provider manifest has an invalid size.");
        }

        using JsonDocument document = JsonDocument.Parse(stream, new JsonDocumentOptions
        {
            AllowTrailingCommas = false,
            CommentHandling = JsonCommentHandling.Disallow,
            MaxDepth = 16,
        });
        JsonElement root = document.RootElement;
        RequireExactString(root, "schema", "foa.sdk.provider_manifest.v1");
        RequireExactNumber(root, "schema_version", 1);
        RequireExactString(root, "provider_id", "game.foa");
        RequireExactString(root, "platform", "windows-x64");
        RequireFalse(root, "conversion_execution_allowed");
        RequireFalse(root, "deployment_execution_allowed");
        RequireFalse(root, "game_launch_allowed");
        RequireFalse(root, "save_access_allowed");

        JsonElement discovery = root.GetProperty("discovery");
        RequireExactString(discovery, "mode", "user-selected-path");
        RequireFalse(discovery, "machine_scan_allowed");
        RequireFalse(discovery, "network_allowed");

        string displayName = RequiredString(root, "display_name");
        string[] capabilities = RequiredStringArray(root, "capabilities");
        string[] runtimeRoutes = root.GetProperty("runtime_routes")
            .EnumerateArray()
            .Select(route => RequiredString(route, "route_id"))
            .ToArray();
        string[] requiredCapabilities =
        {
            "explicit-path-validation",
            "read-only-runtime-observation",
            "compatibility-report",
            "non-mutating-plan-preview",
            "redacted-support-report",
        };
        string[] requiredRuntimeRoutes = { "mono-bepinex5", "il2cpp-bepinex6" };
        if (capabilities.Length != requiredCapabilities.Length
            || !capabilities.ToHashSet(StringComparer.Ordinal).SetEquals(requiredCapabilities)
            || runtimeRoutes.Length != requiredRuntimeRoutes.Length
            || !runtimeRoutes.ToHashSet(StringComparer.Ordinal).SetEquals(requiredRuntimeRoutes))
        {
            throw new InvalidOperationException("The bundled provider manifest is incomplete.");
        }

        return new ProviderManifest("game.foa", displayName, capabilities, runtimeRoutes);
    }

    private static string RequiredString(JsonElement element, string name)
    {
        if (!element.TryGetProperty(name, out JsonElement value)
            || value.ValueKind is not JsonValueKind.String
            || string.IsNullOrWhiteSpace(value.GetString()))
        {
            throw new InvalidOperationException($"The provider manifest requires a non-empty '{name}' string.");
        }
        return value.GetString()!;
    }

    private static string[] RequiredStringArray(JsonElement element, string name)
    {
        if (!element.TryGetProperty(name, out JsonElement value)
            || value.ValueKind is not JsonValueKind.Array)
        {
            throw new InvalidOperationException($"The provider manifest requires a '{name}' array.");
        }
        return value.EnumerateArray()
            .Select(item => item.ValueKind is JsonValueKind.String ? item.GetString() : null)
            .Where(item => !string.IsNullOrWhiteSpace(item))
            .Select(item => item!)
            .Distinct(StringComparer.Ordinal)
            .ToArray();
    }

    private static void RequireExactString(JsonElement element, string name, string expected)
    {
        string actual = RequiredString(element, name);
        if (!string.Equals(actual, expected, StringComparison.Ordinal))
        {
            throw new InvalidOperationException($"The provider manifest has unsupported {name} '{actual}'.");
        }
    }

    private static void RequireExactNumber(JsonElement element, string name, int expected)
    {
        if (!element.TryGetProperty(name, out JsonElement value)
            || value.ValueKind is not JsonValueKind.Number
            || !value.TryGetInt32(out int actual)
            || actual != expected)
        {
            throw new InvalidOperationException($"The provider manifest requires {name} {expected}.");
        }
    }

    private static void RequireFalse(JsonElement element, string name)
    {
        if (!element.TryGetProperty(name, out JsonElement value)
            || value.ValueKind is not JsonValueKind.False)
        {
            throw new InvalidOperationException($"The provider manifest must keep '{name}' false.");
        }
    }
}

internal sealed record SetupManagerOptions(string WorkspaceRoot, string? GameInstallPath);

internal sealed record GameInstallObservation(
    bool IsValid,
    string State,
    string RuntimeRoute,
    string Confidence,
    IReadOnlyList<string> ObservedMarkers,
    IReadOnlyList<string> Blockers);

internal sealed record SetupManagerReadiness(
    bool ProductReady,
    bool WorkspaceReady,
    GameInstallObservation Game,
    IReadOnlyList<string> PassedChecks,
    IReadOnlyList<string> BlockedChecks,
    string PlanPreview);

internal sealed record SetupManagerSaveResult(string ProfilePath, SetupManagerReadiness Readiness);

internal static class SetupManagerCore
{
    private const string ProfileSchema = "foa.sdk.setup_profile.v1";
    private const string ProviderId = "game.foa";
    private const int MaximumProfileBytes = 1024 * 1024;
    private const string InstalledLauncherRelativePath = "bin\\Windows\\profile\\Default\\FOA-SDK.exe";

    public static string DefaultWorkspaceRoot()
    {
        string documents = Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments);
        string baseDirectory = string.IsNullOrWhiteSpace(documents)
            ? Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData)
            : documents;
        return Path.Combine(baseDirectory, "FOA-SDK", "Workspace");
    }

    public static string ProfilePath() => Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
        "FOA-SDK",
        "SetupManager",
        "setup-profile.local.json");

    public static SetupManagerOptions LoadOrDefault(string installRoot)
    {
        string profilePath = ProfilePath();
        if (File.Exists(profilePath))
        {
            return ReadProfile(profilePath, installRoot);
        }

        string legacyPath = Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            "FOA-SDK",
            "ToolWizard",
            "tool-profile.local.json");
        if (File.Exists(legacyPath))
        {
            return ReadLegacyProfile(legacyPath, installRoot);
        }
        return Normalize(new SetupManagerOptions(DefaultWorkspaceRoot(), null), installRoot);
    }

    public static SetupManagerOptions Normalize(SetupManagerOptions options, string installRoot)
    {
        string normalizedInstallRoot = NormalizeRequiredPath(installRoot, "Installation directory");
        string workspaceRoot = NormalizeRequiredPath(options.WorkspaceRoot, "Workspace directory");
        if (IsSameOrChildPath(workspaceRoot, normalizedInstallRoot))
        {
            throw new ArgumentException("The workspace must stay outside the FOA-SDK installation directory.");
        }

        string? gameInstall = NormalizeOptionalPath(options.GameInstallPath, "Fall of Avalon installation");
        if (gameInstall is not null && IsSameOrChildPath(gameInstall, normalizedInstallRoot))
        {
            throw new ArgumentException("The game installation must stay outside the FOA-SDK installation directory.");
        }
        return new SetupManagerOptions(workspaceRoot, gameInstall);
    }

    public static SetupManagerReadiness Evaluate(SetupManagerOptions options, string installRoot)
    {
        _ = ProviderManifestLoader.LoadBundledFoaProvider();
        SetupManagerOptions normalized = Normalize(options, installRoot);
        string normalizedInstallRoot = NormalizeRequiredPath(installRoot, "Installation directory");
        List<string> passed = new();
        List<string> blocked = new();

        bool productReady = File.Exists(Path.Combine(normalizedInstallRoot, "INSTALL_MANIFEST.json"))
            && File.Exists(Path.Combine(normalizedInstallRoot, InstalledLauncherRelativePath));
        if (productReady)
        {
            passed.Add("Installed FOA-SDK product layout is present.");
        }
        else
        {
            blocked.Add("Installed FOA-SDK product files are incomplete. Run installer Repair.");
        }

        RequirePathWithoutExistingReparsePoint(normalized.WorkspaceRoot, "Workspace directory");
        bool workspaceReady = !IsSameOrChildPath(normalized.WorkspaceRoot, normalizedInstallRoot);
        if (workspaceReady)
        {
            passed.Add("Workspace is external to the installed product.");
        }

        GameInstallObservation game = ObserveGameInstall(normalized.GameInstallPath);
        passed.AddRange(game.ObservedMarkers.Select(marker => $"Observed: {marker}"));
        blocked.AddRange(game.Blockers);
        string planPreview = BuildPlanPreview(productReady, workspaceReady, game);
        return new SetupManagerReadiness(
            productReady,
            workspaceReady,
            game,
            passed.Distinct(StringComparer.Ordinal).ToArray(),
            blocked.Distinct(StringComparer.Ordinal).ToArray(),
            planPreview);
    }

    public static SetupManagerSaveResult Save(SetupManagerOptions options, string installRoot)
        => SaveToPath(options, installRoot, ProfilePath());

    public static string ExportSupportReport(
        SetupManagerOptions options,
        string installRoot,
        string destinationPath)
    {
        string fullDestination = Path.GetFullPath(destinationPath);
        if (!string.Equals(Path.GetExtension(fullDestination), ".json", StringComparison.OrdinalIgnoreCase))
        {
            throw new ArgumentException("The support report must use a .json filename.");
        }
        string? parent = Path.GetDirectoryName(fullDestination);
        if (string.IsNullOrWhiteSpace(parent))
        {
            throw new ArgumentException("The support report requires a destination directory.");
        }
        RequirePathWithoutExistingReparsePoint(parent, "Support report destination");
        Directory.CreateDirectory(parent);

        SetupManagerOptions normalized = Normalize(options, installRoot);
        SetupManagerReadiness readiness = Evaluate(normalized, installRoot);
        object report = new
        {
            schema = "foa.sdk.support_report.v1",
            generated_utc = DateTime.UtcNow.ToString("yyyy-MM-ddTHH:mm:ssZ"),
            provider_id = ProviderId,
            install_root = RedactPath(installRoot),
            workspace_root = RedactPath(normalized.WorkspaceRoot),
            game_install_root = normalized.GameInstallPath is null ? null : RedactPath(normalized.GameInstallPath),
            product_ready = readiness.ProductReady,
            workspace_ready = readiness.WorkspaceReady,
            game_state = readiness.Game.State,
            runtime_route = readiness.Game.RuntimeRoute,
            confidence = readiness.Game.Confidence,
            observed_markers = readiness.Game.ObservedMarkers,
            blockers = readiness.BlockedChecks,
            plan_preview = readiness.PlanPreview,
            conversion_execution_allowed = false,
            deployment_execution_allowed = false,
            game_launch_allowed = false,
            save_access_allowed = false,
        };
        AtomicWriteJson(fullDestination, report);
        return fullDestination;
    }

    public static string Describe(SetupManagerReadiness readiness)
    {
        string passed = readiness.PassedChecks.Count == 0
            ? "No checks have passed yet."
            : string.Join(Environment.NewLine, readiness.PassedChecks.Select(value => $"PASS  {value}"));
        string blocked = readiness.BlockedChecks.Count == 0
            ? "No readiness blockers."
            : string.Join(Environment.NewLine, readiness.BlockedChecks.Select(value => $"WAIT  {value}"));
        return $"Product: {(readiness.ProductReady ? "ready" : "repair required")}{Environment.NewLine}"
            + $"Workspace: {(readiness.WorkspaceReady ? "ready" : "blocked")}{Environment.NewLine}"
            + $"Game: {readiness.Game.State}{Environment.NewLine}"
            + $"Runtime observation: {readiness.Game.RuntimeRoute} ({readiness.Game.Confidence}){Environment.NewLine}{Environment.NewLine}"
            + $"{passed}{Environment.NewLine}{Environment.NewLine}{blocked}{Environment.NewLine}{Environment.NewLine}"
            + readiness.PlanPreview;
    }

    public static int RunSelfTest()
    {
        string root = Path.Combine(Path.GetTempPath(), "foa-sdk-control-panel-self-test", Guid.NewGuid().ToString("N"));
        try
        {
            string install = Path.Combine(root, "installed");
            string workspace = Path.Combine(root, "workspace");
            string game = Path.Combine(root, "game");
            string data = Path.Combine(game, "TaintedGrail_Data");
            Directory.CreateDirectory(Path.Combine(install, "bin", "Windows", "profile", "Default"));
            Directory.CreateDirectory(Path.Combine(data, "Managed"));
            File.WriteAllText(Path.Combine(install, "INSTALL_MANIFEST.json"), "{}\n");
            File.WriteAllBytes(Path.Combine(install, InstalledLauncherRelativePath), new byte[] { 0x4d, 0x5a });
            File.WriteAllBytes(Path.Combine(game, "UnityPlayer.dll"), new byte[] { 0x4d, 0x5a });
            File.WriteAllBytes(Path.Combine(data, "Managed", "Assembly-CSharp.dll"), new byte[] { 0x4d, 0x5a });

            SetupManagerOptions options = new(workspace, game);
            SetupManagerReadiness readiness = Evaluate(options, install);
            if (!readiness.ProductReady || !readiness.WorkspaceReady || !readiness.Game.IsValid
                || readiness.Game.RuntimeRoute != "mono-indicated")
            {
                return 1;
            }

            string profile = Path.Combine(root, "profile.json");
            SaveToPath(options, install, profile);
            SetupManagerOptions loaded = ReadProfile(profile, install);
            if (!string.Equals(loaded.GameInstallPath, Path.GetFullPath(game), StringComparison.OrdinalIgnoreCase))
            {
                return 1;
            }

            string report = Path.Combine(root, "support-report.json");
            ExportSupportReport(options, install, report);
            string reportText = File.ReadAllText(report);
            return reportText.Contains(game, StringComparison.OrdinalIgnoreCase) ? 1 : 0;
        }
        finally
        {
            try
            {
                if (Directory.Exists(root))
                {
                    Directory.Delete(root, recursive: true);
                }
            }
            catch (IOException)
            {
            }
            catch (UnauthorizedAccessException)
            {
            }
        }
    }

    private static SetupManagerSaveResult SaveToPath(
        SetupManagerOptions options,
        string installRoot,
        string profilePath)
    {
        SetupManagerOptions normalized = Normalize(options, installRoot);
        SetupManagerReadiness readiness = Evaluate(normalized, installRoot);
        RequirePathWithoutExistingReparsePoint(normalized.WorkspaceRoot, "Workspace directory");
        Directory.CreateDirectory(normalized.WorkspaceRoot);
        object profile = new
        {
            schema = ProfileSchema,
            schema_version = 1,
            provider_id = ProviderId,
            saved_utc = DateTime.UtcNow.ToString("yyyy-MM-ddTHH:mm:ssZ"),
            install_root = NormalizeRequiredPath(installRoot, "Installation directory"),
            workspace_root = normalized.WorkspaceRoot,
            game_install_path = normalized.GameInstallPath,
            game_state = readiness.Game.State,
            runtime_route = readiness.Game.RuntimeRoute,
            confidence = readiness.Game.Confidence,
            observed_markers = readiness.Game.ObservedMarkers,
            blockers = readiness.BlockedChecks,
            conversion_execution_allowed = false,
            deployment_execution_allowed = false,
            game_launch_allowed = false,
            save_access_allowed = false,
        };
        AtomicWriteJson(profilePath, profile);
        return new SetupManagerSaveResult(profilePath, readiness);
    }

    private static SetupManagerOptions ReadProfile(string path, string installRoot)
    {
        using JsonDocument document = ReadBoundedJson(path, "Setup Manager profile");
        JsonElement root = document.RootElement;
        string schema = StringValue(root, "schema") ?? string.Empty;
        if (!string.Equals(schema, ProfileSchema, StringComparison.Ordinal))
        {
            throw new InvalidOperationException(
                $"The saved Setup Manager profile uses unsupported schema '{schema}'.");
        }
        if (!root.TryGetProperty("schema_version", out JsonElement version)
            || !version.TryGetInt32(out int versionNumber)
            || versionNumber != 1)
        {
            throw new InvalidOperationException("The saved Setup Manager profile requires schema version 1.");
        }
        if (!string.Equals(StringValue(root, "provider_id"), ProviderId, StringComparison.Ordinal))
        {
            throw new InvalidOperationException("The saved Setup Manager profile uses an unsupported provider.");
        }
        string workspace = StringValue(root, "workspace_root") ?? DefaultWorkspaceRoot();
        return Normalize(new SetupManagerOptions(workspace, StringValue(root, "game_install_path")), installRoot);
    }

    private static SetupManagerOptions ReadLegacyProfile(string path, string installRoot)
    {
        using JsonDocument document = ReadBoundedJson(path, "legacy Tool Wizard profile");
        JsonElement root = document.RootElement;
        string workspace = StringValue(root, "workspace_root") ?? DefaultWorkspaceRoot();
        string? gameInstall = StringValue(root, "tainted_grail_install_path");
        return Normalize(new SetupManagerOptions(workspace, gameInstall), installRoot);
    }

    private static JsonDocument ReadBoundedJson(string path, string label)
    {
        FileInfo file = new(path);
        if (!file.Exists || file.Length <= 0 || file.Length > MaximumProfileBytes
            || (file.Attributes & FileAttributes.ReparsePoint) != 0)
        {
            throw new InvalidOperationException($"The {label} is missing, unsafe, or too large.");
        }
        return JsonDocument.Parse(File.ReadAllBytes(path), new JsonDocumentOptions
        {
            AllowTrailingCommas = false,
            CommentHandling = JsonCommentHandling.Disallow,
            MaxDepth = 32,
        });
    }

    private static GameInstallObservation ObserveGameInstall(string? path)
    {
        if (path is null)
        {
            return new GameInstallObservation(
                false,
                "not configured",
                "unknown",
                "no local evidence",
                Array.Empty<string>(),
                new[] { "Choose the local Fall of Avalon folder to run read-only validation." });
        }

        RequirePathWithoutExistingReparsePoint(path, "Fall of Avalon installation");
        if (!Directory.Exists(path))
        {
            return new GameInstallObservation(
                false,
                "path missing",
                "unknown",
                "no local evidence",
                Array.Empty<string>(),
                new[] { "The selected Fall of Avalon folder does not exist." });
        }

        List<string> markers = new();
        if (File.Exists(Path.Combine(path, "UnityPlayer.dll")))
        {
            markers.Add("UnityPlayer.dll");
        }
        if (File.Exists(Path.Combine(path, "TaintedGrail.exe"))
            || File.Exists(Path.Combine(path, "Fall of Avalon.exe")))
        {
            markers.Add("game executable");
        }

        string? dataDirectory = new[] { "TaintedGrail_Data", "Fall of Avalon_Data" }
            .Select(name => Path.Combine(path, name))
            .FirstOrDefault(Directory.Exists);
        if (dataDirectory is not null)
        {
            markers.Add("Unity data directory");
        }
        bool valid = markers.Count > 0 && dataDirectory is not null;
        if (!valid)
        {
            return new GameInstallObservation(
                false,
                "not recognized",
                "unknown",
                "insufficient markers",
                markers,
                new[] { "The selected folder does not contain the expected Fall of Avalon Unity layout." });
        }

        bool mono = File.Exists(Path.Combine(dataDirectory!, "Managed", "Assembly-CSharp.dll"));
        bool il2cpp = File.Exists(Path.Combine(path, "GameAssembly.dll"))
            && File.Exists(Path.Combine(dataDirectory!, "il2cpp_data", "Metadata", "global-metadata.dat"));
        if (mono)
        {
            markers.Add("managed Assembly-CSharp.dll");
        }
        if (il2cpp)
        {
            markers.Add("IL2CPP GameAssembly and metadata");
        }

        string route = mono == il2cpp ? "unknown" : mono ? "mono-indicated" : "il2cpp-indicated";
        string confidence = mono == il2cpp ? "ambiguous or incomplete markers" : "indicated, not runtime verified";
        List<string> blockers = new()
        {
            "Game files remain read-only; deployment, loader installation, launch, and save access are unavailable.",
            "Runtime compatibility requires exact route-specific evidence before execution can be enabled.",
        };
        if (route == "unknown")
        {
            blockers.Add("The local runtime route could not be classified from the bounded marker set.");
        }
        return new GameInstallObservation(true, "recognized", route, confidence, markers, blockers);
    }

    private static string BuildPlanPreview(
        bool productReady,
        bool workspaceReady,
        GameInstallObservation game)
    {
        string state = productReady && workspaceReady && game.IsValid
            ? "READ-ONLY READY"
            : "NEEDS ATTENTION";
        return $"Plan preview: {state}{Environment.NewLine}"
            + "1. Keep FOA-SDK product lifecycle under Windows Installer.\n"
            + "2. Save this local profile and external workspace binding.\n"
            + "3. Keep the selected game directory read-only.\n"
            + "4. Require a separately reviewed executor before any deployment or launch action.";
    }

    private static void AtomicWriteJson(string path, object document)
    {
        string fullPath = Path.GetFullPath(path);
        string? directory = Path.GetDirectoryName(fullPath);
        if (string.IsNullOrWhiteSpace(directory))
        {
            throw new ArgumentException("A JSON output path requires a directory.");
        }
        Directory.CreateDirectory(directory);
        RequirePathWithoutExistingReparsePoint(directory, "JSON output directory");
        string temporary = Path.Combine(directory, $".{Path.GetFileName(fullPath)}.{Guid.NewGuid():N}.tmp");
        try
        {
            JsonSerializerOptions options = new() { WriteIndented = true };
            File.WriteAllText(temporary, JsonSerializer.Serialize(document, options) + Environment.NewLine);
            File.Move(temporary, fullPath, overwrite: true);
        }
        finally
        {
            if (File.Exists(temporary))
            {
                File.Delete(temporary);
            }
        }
    }

    private static string RedactPath(string path)
    {
        string fullPath = Path.GetFullPath(path);
        string leaf = Path.GetFileName(Path.TrimEndingDirectorySeparator(fullPath));
        byte[] digest = System.Security.Cryptography.SHA256.HashData(System.Text.Encoding.UTF8.GetBytes(fullPath));
        return $"{(string.IsNullOrWhiteSpace(leaf) ? "root" : leaf)}#{Convert.ToHexString(digest)[..12].ToLowerInvariant()}";
    }

    private static string NormalizeRequiredPath(string value, string label)
    {
        string expanded = Environment.ExpandEnvironmentVariables(value.Trim());
        if (string.IsNullOrWhiteSpace(expanded) || !Path.IsPathFullyQualified(expanded))
        {
            throw new ArgumentException($"{label} must be an absolute Windows path.");
        }
        string fullPath = Path.TrimEndingDirectorySeparator(Path.GetFullPath(expanded));
        string? root = Path.GetPathRoot(fullPath);
        if (string.IsNullOrWhiteSpace(root)
            || string.Equals(fullPath, Path.TrimEndingDirectorySeparator(root), StringComparison.OrdinalIgnoreCase))
        {
            throw new ArgumentException($"{label} must not be a filesystem root.");
        }
        return fullPath;
    }

    private static string? NormalizeOptionalPath(string? value, string label)
        => string.IsNullOrWhiteSpace(value) ? null : NormalizeRequiredPath(value, label);

    private static void RequirePathWithoutExistingReparsePoint(string path, string label)
    {
        string current = Path.GetFullPath(path);
        while (!string.IsNullOrWhiteSpace(current))
        {
            if ((File.Exists(current) || Directory.Exists(current))
                && (File.GetAttributes(current) & FileAttributes.ReparsePoint) != 0)
            {
                throw new InvalidOperationException(
                    $"{label} must not traverse a symbolic link, junction, or reparse point: {current}");
            }
            string? parent = Directory.GetParent(current)?.FullName;
            if (parent is null || string.Equals(parent, current, StringComparison.OrdinalIgnoreCase))
            {
                break;
            }
            current = parent;
        }
    }

    private static bool IsSameOrChildPath(string candidate, string parent)
    {
        string normalizedCandidate = Path.TrimEndingDirectorySeparator(Path.GetFullPath(candidate));
        string normalizedParent = Path.TrimEndingDirectorySeparator(Path.GetFullPath(parent));
        return string.Equals(normalizedCandidate, normalizedParent, StringComparison.OrdinalIgnoreCase)
            || normalizedCandidate.StartsWith(normalizedParent + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase);
    }

    private static string? StringValue(JsonElement root, string propertyName)
        => root.TryGetProperty(propertyName, out JsonElement value)
            && value.ValueKind is JsonValueKind.String
                ? value.GetString()
                : null;
}
