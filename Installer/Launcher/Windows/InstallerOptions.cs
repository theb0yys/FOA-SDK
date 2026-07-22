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
    InstallerOperation Operation,
    bool Quiet,
    bool SmokeTest,
    bool LaunchAfterInstall,
    bool NoDialog)
{
    public static InstallerOptions Parse(string[] args)
    {
        string? msiPath = null;
        string installRoot = DefaultInstallRoot();
        InstallerOperation operation = InstallerOperation.InstallOrUpgrade;
        bool quiet = false;
        bool smokeTest = false;
        bool launchAfterInstall = false;
        bool noDialog = false;

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
                case "--no-dialog":
                    noDialog = true;
                    break;
                case "--help":
                case "-h":
                    throw new ArgumentException(HelpText());
                default:
                    throw new ArgumentException($"Unknown option: {current}\n\n{HelpText()}");
            }
        }

        return new InstallerOptions(
            msiPath,
            NormalizeInstallRoot(installRoot),
            operation,
            quiet,
            smokeTest,
            launchAfterInstall,
            noDialog);
    }

    public static bool WantsConsoleError(string[] args) =>
        args.Contains("--quiet", StringComparer.Ordinal)
        || args.Contains("--smoke-test", StringComparer.Ordinal)
        || args.Contains("--no-dialog", StringComparer.Ordinal);

    public static string NormalizeInstallRoot(string value)
    {
        string expanded = Environment.ExpandEnvironmentVariables(value.Trim());
        if (string.IsNullOrWhiteSpace(expanded) || !Path.IsPathFullyQualified(expanded))
        {
            throw new ArgumentException("The installation directory must be an absolute Windows path.");
        }

        string fullPath = Path.TrimEndingDirectorySeparator(Path.GetFullPath(expanded));
        string? root = Path.GetPathRoot(fullPath);
        if (string.IsNullOrWhiteSpace(root)
            || string.Equals(fullPath, Path.TrimEndingDirectorySeparator(root), StringComparison.OrdinalIgnoreCase))
        {
            throw new ArgumentException("The installation directory must not be a filesystem root.");
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
        + "[--quiet] [--smoke-test] [--launch-after-install|--no-launch-after-install] [--no-dialog]";

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
