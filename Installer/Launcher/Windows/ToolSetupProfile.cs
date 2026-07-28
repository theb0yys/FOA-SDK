// SPDX-License-Identifier: Apache-2.0 OR MIT
using System.Globalization;
using System.Text.Json;

namespace FOA.SDK.InstallerLauncher;

internal sealed record ToolSetupOptions(
    string WorkspaceRoot,
    string? O3deEditorPath,
    string? UnityEditorPath,
    string? UnityProjectPath,
    string? TaintedGrailInstallPath)
{
    public static ToolSetupOptions Default() => new(
        ToolSetupProfile.DefaultWorkspaceRoot(),
        null,
        null,
        null,
        null);
}

internal sealed record ToolSetupReadiness(
    bool ReadyForAuthoring,
    bool ReadyForConversionPreview,
    bool ReadyForDeploymentPreview,
    IReadOnlyList<string> PassedChecks,
    IReadOnlyList<string> BlockedChecks,
    string UnityBatchCommandPreview,
    string DeploymentTargetPreview);

internal sealed record ToolSetupSaveResult(
    string ProfilePath,
    ToolSetupReadiness Readiness);

internal static class ToolSetupProfile
{
    private const string ProductConfigDirectoryName = "FOA-SDK";
    private const string ToolWizardDirectoryName = "ToolWizard";
    private const string ProfileFileName = "tool-profile.local.json";

    public static string DefaultWorkspaceRoot()
    {
        string documents = Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments);
        string baseDirectory = !string.IsNullOrWhiteSpace(documents)
            ? documents
            : Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
        return Path.Combine(baseDirectory, "FOA-SDK", "Workspace");
    }

    public static ToolSetupOptions LoadOrDefault(string installRoot)
    {
        ToolSetupOptions defaults = ToolSetupOptions.Default();
        string profilePath = ProfilePath();
        if (!File.Exists(profilePath))
        {
            return Normalize(defaults, installRoot);
        }

        try
        {
            using JsonDocument document = JsonDocument.Parse(File.ReadAllText(profilePath));
            JsonElement root = document.RootElement;
            return Normalize(
                new ToolSetupOptions(
                    StringValue(root, "workspace_root") ?? defaults.WorkspaceRoot,
                    StringValue(root, "o3de_editor_path"),
                    StringValue(root, "unity_editor_path"),
                    StringValue(root, "unity_conversion_project_path"),
                    StringValue(root, "tainted_grail_install_path")),
                installRoot);
        }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException or JsonException or ArgumentException)
        {
            return Normalize(defaults, installRoot);
        }
    }

    public static ToolSetupOptions Normalize(ToolSetupOptions options, string installRoot)
    {
        string normalizedInstallRoot = NormalizeRequiredPath(installRoot, "Installation directory");
        string workspaceRoot = NormalizeRequiredPath(options.WorkspaceRoot, "Workspace directory");
        if (IsSameOrChildPath(workspaceRoot, normalizedInstallRoot))
        {
            throw new ArgumentException(
                "The workspace directory must stay outside the FOA-SDK installation directory.");
        }

        return options with
        {
            WorkspaceRoot = workspaceRoot,
            O3deEditorPath = NormalizeOptionalPath(options.O3deEditorPath, "O3DE Editor executable"),
            UnityEditorPath = NormalizeOptionalPath(options.UnityEditorPath, "Unity Editor executable"),
            UnityProjectPath = NormalizeOptionalPath(options.UnityProjectPath, "Unity conversion project"),
            TaintedGrailInstallPath = NormalizeOptionalPath(options.TaintedGrailInstallPath, "Tainted Grail install"),
        };
    }

    public static ToolSetupReadiness Validate(ToolSetupOptions options, string installRoot)
    {
        ToolSetupOptions normalized = Normalize(options, installRoot);
        RequirePathWithoutExistingReparsePoint(normalized.WorkspaceRoot, "Workspace directory");
        List<string> passed = new()
        {
            $"Workspace binding is external: {normalized.WorkspaceRoot}",
        };
        List<string> blocked = new();

        bool hasO3deEditor = ValidateOptionalFile(
            normalized.O3deEditorPath,
            "O3DE Editor executable",
            "Editor.exe",
            "Select an O3DE Editor.exe path before authoring launch is ready.",
            passed,
            blocked);
        bool hasUnityEditor = ValidateOptionalFile(
            normalized.UnityEditorPath,
            "Unity Editor executable",
            "Unity.exe",
            "Select a Unity Editor executable before conversion preview is ready.",
            passed,
            blocked);
        bool hasUnityProject = ValidateOptionalUnityProject(
            normalized.UnityProjectPath,
            passed,
            blocked);
        bool hasTaintedGrailInstall = ValidateOptionalTaintedGrailInstall(
            normalized.TaintedGrailInstallPath,
            passed,
            blocked);

        string unityPreview = hasUnityEditor && hasUnityProject
            ? $"{Quote(normalized.UnityEditorPath!)} -batchmode -projectPath {Quote(normalized.UnityProjectPath!)} -executeMethod FOA.SDK.UnityProvider.DescribeCapabilities -quit"
            : "Unity batch command preview unavailable until Unity Editor and conversion project are selected.";
        string deploymentPreview = hasTaintedGrailInstall
            ? $"Manual deployment preview target: {normalized.TaintedGrailInstallPath}"
            : "Deployment preview unavailable until a local Tainted Grail install is selected.";

        bool readyForAuthoring = hasO3deEditor;
        bool readyForConversionPreview = readyForAuthoring && hasUnityEditor && hasUnityProject;
        bool readyForDeploymentPreview = hasTaintedGrailInstall;
        if (!readyForAuthoring)
        {
            blocked.Add("Authoring launch is blocked until O3DE Editor is selected.");
        }
        if (!readyForConversionPreview)
        {
            blocked.Add("Unity conversion remains preview-only until O3DE, Unity, and conversion project paths validate.");
        }
        if (!readyForDeploymentPreview)
        {
            blocked.Add("Deployment review remains blocked until a Tainted Grail install path validates.");
        }

        return new ToolSetupReadiness(
            readyForAuthoring,
            readyForConversionPreview,
            readyForDeploymentPreview,
            passed,
            blocked.Distinct(StringComparer.Ordinal).ToArray(),
            unityPreview,
            deploymentPreview);
    }

    public static ToolSetupSaveResult Save(ToolSetupOptions options, string installRoot)
    {
        ToolSetupOptions normalized = Normalize(options, installRoot);
        ToolSetupReadiness readiness = Validate(normalized, installRoot);
        Directory.CreateDirectory(normalized.WorkspaceRoot);
        string configRoot = Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            ProductConfigDirectoryName,
            ToolWizardDirectoryName);
        Directory.CreateDirectory(configRoot);
        string profilePath = ProfilePath();
        string timestamp = DateTime.UtcNow.ToString("yyyy-MM-ddTHH:mm:ssZ", CultureInfo.InvariantCulture);
        object document = new
        {
            schema = "foa.sdk.tool_profile.v1",
            saved_utc = timestamp,
            install_root = NormalizeRequiredPath(installRoot, "Installation directory"),
            workspace_root = normalized.WorkspaceRoot,
            o3de_editor_path = normalized.O3deEditorPath,
            unity_editor_path = normalized.UnityEditorPath,
            unity_conversion_project_path = normalized.UnityProjectPath,
            tainted_grail_install_path = normalized.TaintedGrailInstallPath,
            ready_for_authoring = readiness.ReadyForAuthoring,
            ready_for_conversion_preview = readiness.ReadyForConversionPreview,
            ready_for_deployment_preview = readiness.ReadyForDeploymentPreview,
            passed_checks = readiness.PassedChecks,
            blocked_checks = readiness.BlockedChecks,
            unity_batch_command_preview = readiness.UnityBatchCommandPreview,
            deployment_target_preview = readiness.DeploymentTargetPreview,
            conversion_execution_allowed = false,
            deployment_execution_allowed = false,
        };
        JsonSerializerOptions jsonOptions = new() { WriteIndented = true };
        File.WriteAllText(profilePath, JsonSerializer.Serialize(document, jsonOptions) + Environment.NewLine);
        return new ToolSetupSaveResult(profilePath, readiness);
    }

    public static string Describe(ToolSetupOptions options, ToolSetupReadiness readiness)
    {
        string passed = readiness.PassedChecks.Count == 0
            ? "None yet."
            : string.Join(Environment.NewLine, readiness.PassedChecks.Select(check => $"- {check}"));
        string blocked = readiness.BlockedChecks.Count == 0
            ? "None."
            : string.Join(Environment.NewLine, readiness.BlockedChecks.Select(check => $"- {check}"));
        return $"Workspace directory: {options.WorkspaceRoot}{Environment.NewLine}"
            + $"O3DE Editor: {DisplayPath(options.O3deEditorPath)}{Environment.NewLine}"
            + $"Unity Editor: {DisplayPath(options.UnityEditorPath)}{Environment.NewLine}"
            + $"Unity conversion project: {DisplayPath(options.UnityProjectPath)}{Environment.NewLine}"
            + $"Tainted Grail install: {DisplayPath(options.TaintedGrailInstallPath)}{Environment.NewLine}{Environment.NewLine}"
            + $"Readiness checks:{Environment.NewLine}{passed}{Environment.NewLine}{Environment.NewLine}"
            + $"Blocked or pending:{Environment.NewLine}{blocked}{Environment.NewLine}{Environment.NewLine}"
            + $"Unity preview command: {readiness.UnityBatchCommandPreview}{Environment.NewLine}"
            + $"Deployment preview: {readiness.DeploymentTargetPreview}";
    }

    public static string ProfilePath() => Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
        ProductConfigDirectoryName,
        ToolWizardDirectoryName,
        ProfileFileName);

    private static bool ValidateOptionalFile(
        string? path,
        string label,
        string expectedFileName,
        string missingMessage,
        List<string> passed,
        List<string> blocked)
    {
        if (path is null)
        {
            blocked.Add(missingMessage);
            return false;
        }
        string file = RequireRegularFile(path, label);
        if (!string.Equals(Path.GetFileName(file), expectedFileName, StringComparison.OrdinalIgnoreCase))
        {
            throw new ArgumentException($"{label} must be named {expectedFileName}.");
        }
        passed.Add($"{label} validated: {file}");
        return true;
    }

    private static bool ValidateOptionalUnityProject(string? path, List<string> passed, List<string> blocked)
    {
        if (path is null)
        {
            blocked.Add("Select a Unity conversion project before conversion preview is ready.");
            return false;
        }
        string directory = RequireExistingDirectory(path, "Unity conversion project");
        RequireRegularFile(Path.Combine(directory, "ProjectSettings", "ProjectVersion.txt"), "Unity project version");
        RequireExistingDirectory(Path.Combine(directory, "Assets"), "Unity project Assets directory");
        passed.Add($"Unity conversion project validated: {directory}");
        return true;
    }

    private static bool ValidateOptionalTaintedGrailInstall(string? path, List<string> passed, List<string> blocked)
    {
        if (path is null)
        {
            blocked.Add("Select a local Tainted Grail install path before deployment review is ready.");
            return false;
        }
        string directory = RequireExistingDirectory(path, "Tainted Grail install");
        if (!File.Exists(Path.Combine(directory, "UnityPlayer.dll"))
            && !Directory.Exists(Path.Combine(directory, "TaintedGrail_Data"))
            && !File.Exists(Path.Combine(directory, "TaintedGrail.exe")))
        {
            throw new ArgumentException(
                "Tainted Grail install must contain UnityPlayer.dll, TaintedGrail_Data, or TaintedGrail.exe.");
        }
        passed.Add($"Tainted Grail install validated: {directory}");
        return true;
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
    {
        if (string.IsNullOrWhiteSpace(value))
        {
            return null;
        }
        return NormalizeRequiredPath(value, label);
    }

    private static string RequireRegularFile(string path, string label)
    {
        RequirePathWithoutExistingReparsePoint(path, label);
        if (!File.Exists(path))
        {
            throw new InvalidOperationException($"{label} is missing or is not a regular file: {path}");
        }
        return path;
    }

    private static string RequireExistingDirectory(string path, string label)
    {
        RequirePathWithoutExistingReparsePoint(path, label);
        if (!Directory.Exists(path))
        {
            throw new InvalidOperationException($"{label} is missing or is not a directory: {path}");
        }
        return path;
    }

    private static void RequirePathWithoutExistingReparsePoint(string path, string label)
    {
        string current = path;
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
        string normalizedCandidate = Path.GetFullPath(candidate);
        string normalizedParent = Path.GetFullPath(parent);
        if (string.Equals(
                Path.TrimEndingDirectorySeparator(normalizedCandidate),
                Path.TrimEndingDirectorySeparator(normalizedParent),
                StringComparison.OrdinalIgnoreCase))
        {
            return true;
        }
        string parentWithSeparator = Path.EndsInDirectorySeparator(normalizedParent)
            ? normalizedParent
            : normalizedParent + Path.DirectorySeparatorChar;
        return normalizedCandidate.StartsWith(parentWithSeparator, StringComparison.OrdinalIgnoreCase);
    }

    private static string? StringValue(JsonElement root, string propertyName)
    {
        if (root.TryGetProperty(propertyName, out JsonElement value)
            && value.ValueKind is JsonValueKind.String)
        {
            return value.GetString();
        }
        return null;
    }

    private static string DisplayPath(string? path) => path ?? "not selected";

    private static string Quote(string value) => $"\"{value.Replace("\"", "\\\"")}\"";
}
