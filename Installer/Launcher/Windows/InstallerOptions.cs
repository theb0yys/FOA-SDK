// SPDX-License-Identifier: Apache-2.0 OR MIT
namespace FOA.SDK.InstallerLauncher;

internal enum InstallerOperation
{
    InstallOrUpgrade,
    Repair,
    Uninstall,
}

internal sealed record InstallerOptions(
    string? MsiPath,
    string InstallRoot,
    string? EvidenceRoot,
    InstallerOperation Operation,
    bool Quiet,
    bool SmokeTest,
    bool LaunchAfterInstall,
    bool OpenToolWizardAfterInstall,
    bool ToolWizardOnly,
    bool NoDialog,
    bool SaveToolProfile,
    string? WorkspaceRoot,
    string? O3deEditorPath,
    string? UnityEditorPath,
    string? UnityProjectPath,
    string? TaintedGrailInstallPath)
{
    public static InstallerOptions Parse(string[] args)
    {
        string? msiPath = null;
        string installRoot = DefaultInstallRoot();
        string? evidenceRoot = null;
        InstallerOperation operation = InstallerOperation.InstallOrUpgrade;
        bool quiet = false;
        bool smokeTest = false;
        bool launchAfterInstall = false;
        bool openToolWizardAfterInstall = true;
        bool toolWizardOnly = false;
        bool noDialog = false;
        bool saveToolProfile = false;
        string? workspaceRoot = null;
        string? o3deEditorPath = null;
        string? unityEditorPath = null;
        string? unityProjectPath = null;
        string? taintedGrailInstallPath = null;

        for (int index = 0; index < args.Length; index++)
        {
            string current = args[index];
            switch (current)
            {
                case "--msi":
                    msiPath = RequireValue(args, ref index, current);
                    break;
                case "--install-root":
                    installRoot = NormalizeInstallRoot(RequireValue(args, ref index, current));
                    break;
                case "--evidence-root":
                    evidenceRoot = NormalizeDirectoryPath(RequireValue(args, ref index, current), "Evidence directory");
                    break;
                case "--operation":
                    operation = ParseOperation(RequireValue(args, ref index, current));
                    break;
                case "--quiet":
                    quiet = true;
                    noDialog = true;
                    break;
                case "--smoke-test":
                    smokeTest = true;
                    noDialog = true;
                    break;
                case "--launch-after-install":
                    launchAfterInstall = true;
                    break;
                case "--no-launch-after-install":
                    launchAfterInstall = false;
                    break;
                case "--open-tool-wizard-after-install":
                    openToolWizardAfterInstall = true;
                    break;
                case "--no-open-tool-wizard-after-install":
                    openToolWizardAfterInstall = false;
                    break;
                case "--tool-wizard":
                    toolWizardOnly = true;
                    break;
                case "--no-dialog":
                    noDialog = true;
                    break;
                case "--save-tool-profile":
                    saveToolProfile = true;
                    noDialog = true;
                    break;
                case "--workspace-root":
                    workspaceRoot = RequireValue(args, ref index, current);
                    break;
                case "--o3de-editor":
                    o3deEditorPath = RequireValue(args, ref index, current);
                    break;
                case "--unity-editor":
                    unityEditorPath = RequireValue(args, ref index, current);
                    break;
                case "--unity-project":
                    unityProjectPath = RequireValue(args, ref index, current);
                    break;
                case "--tainted-grail-install":
                    taintedGrailInstallPath = RequireValue(args, ref index, current);
                    break;
                case "--help":
                case "-h":
                    throw new ArgumentException(HelpText());
                default:
                    throw new ArgumentException($"Unknown option: {current}\n\n{HelpText()}");
            }
        }

        if (saveToolProfile && !toolWizardOnly)
        {
            throw new ArgumentException("--save-tool-profile must be used with --tool-wizard.");
        }

        return new InstallerOptions(
            msiPath,
            NormalizeInstallRoot(installRoot),
            evidenceRoot,
            operation,
            quiet,
            smokeTest,
            launchAfterInstall,
            openToolWizardAfterInstall,
            toolWizardOnly,
            noDialog,
            saveToolProfile,
            workspaceRoot,
            o3deEditorPath,
            unityEditorPath,
            unityProjectPath,
            taintedGrailInstallPath);
    }

    public static bool WantsConsoleError(string[] args) =>
        args.Contains("--quiet", StringComparer.Ordinal)
        || args.Contains("--smoke-test", StringComparer.Ordinal)
        || args.Contains("--no-dialog", StringComparer.Ordinal)
        || args.Contains("--save-tool-profile", StringComparer.Ordinal);

    public static string NormalizeInstallRoot(string value)
        => NormalizeDirectoryPath(value, "The installation directory");

    public static string NormalizeDirectoryPath(string value, string label)
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

    public static string DefaultInstallRoot() => Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
        "Programs",
        "Tainted Grail FoA SDK");

    public static string HelpText() =>
        "Usage: FOA-SDK-Installer.exe [--msi <reviewed.msi>] "
        + "[--install-root <directory>] [--operation install|repair|uninstall] "
        + "[--evidence-root <directory>] "
        + "[--quiet] [--smoke-test] [--launch-after-install|--no-launch-after-install] "
        + "[--open-tool-wizard-after-install|--no-open-tool-wizard-after-install] "
        + "[--tool-wizard] [--save-tool-profile] [--workspace-root <directory>] "
        + "[--o3de-editor <Editor.exe>] [--unity-editor <Unity.exe>] "
        + "[--unity-project <directory>] [--tainted-grail-install <directory>] [--no-dialog]";

    private static InstallerOperation ParseOperation(string value) => value.ToLowerInvariant() switch
    {
        "install" or "upgrade" => InstallerOperation.InstallOrUpgrade,
        "repair" => InstallerOperation.Repair,
        "uninstall" => InstallerOperation.Uninstall,
        _ => throw new ArgumentException("--operation must be install, upgrade, repair, or uninstall."),
    };

    private static string RequireValue(string[] args, ref int index, string option)
    {
        if (index + 1 >= args.Length || args[index + 1].StartsWith("--", StringComparison.Ordinal))
        {
            throw new ArgumentException($"{option} requires a value.\n\n{HelpText()}");
        }
        return args[++index];
    }
}
