// SPDX-License-Identifier: Apache-2.0 OR MIT
using System.Diagnostics;

namespace FOA.SDK.InstallerLauncher;

internal sealed record InstallerRunResult(bool Succeeded, int ExitCode, bool RebootRequired, string Message, string LogPath);

internal static class WindowsInstallerRunner
{
    private static readonly HashSet<int> SuccessfulExitCodes = new() { 0, 1641, 3010 };

    public static async Task<InstallerRunResult> RunAsync(InstallerPayload payload, InstallerOptions options)
    {
        ValidateInstallRoot(options.InstallRoot);
        string windowsInstallerPath = ResolveWindowsInstallerPath();
        string logDirectory = Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            "FOA-SDK",
            "Installer",
            "Logs");
        Directory.CreateDirectory(logDirectory);
        string logPath = Path.Combine(
            logDirectory,
            $"{DateTime.UtcNow:yyyyMMddTHHmmssZ}-{options.Operation.ToString().ToLowerInvariant()}-{Guid.NewGuid():N}.log");

        ProcessStartInfo startInfo = new()
        {
            FileName = windowsInstallerPath,
            UseShellExecute = false,
            CreateNoWindow = true,
            WorkingDirectory = payload.MsiFile.DirectoryName ?? AppContext.BaseDirectory,
        };
        startInfo.ArgumentList.Add(options.Operation switch
        {
            InstallerOperation.InstallOrUpgrade => "/i",
            InstallerOperation.Repair => "/fa",
            InstallerOperation.Uninstall => "/x",
            _ => throw new InvalidOperationException("Unsupported installer operation."),
        });
        startInfo.ArgumentList.Add(payload.MsiFile.FullName);
        startInfo.ArgumentList.Add("/qn");
        startInfo.ArgumentList.Add("/norestart");
        startInfo.ArgumentList.Add($"INSTALL_ROOT={options.InstallRoot}");
        startInfo.ArgumentList.Add("/l*v");
        startInfo.ArgumentList.Add(logPath);

        using Process process = Process.Start(startInfo)
            ?? throw new InvalidOperationException("Windows Installer did not start.");
        await process.WaitForExitAsync();

        int exitCode = process.ExitCode;
        bool succeeded = SuccessfulExitCodes.Contains(exitCode);
        bool rebootRequired = exitCode is 1641 or 3010;
        string operation = options.Operation switch
        {
            InstallerOperation.InstallOrUpgrade => "Installation or upgrade",
            InstallerOperation.Repair => "Repair",
            InstallerOperation.Uninstall => "Uninstall",
            _ => "Installer operation",
        };
        string message = succeeded
            ? rebootRequired
                ? $"{operation} completed; Windows requested a restart."
                : $"{operation} completed successfully."
            : $"{operation} failed with Windows Installer exit code {exitCode}.";
        return new InstallerRunResult(succeeded, exitCode, rebootRequired, message, logPath);
    }

    private static string ResolveWindowsInstallerPath()
    {
        string systemDirectory = Environment.SystemDirectory;
        if (string.IsNullOrWhiteSpace(systemDirectory)
            || !Path.IsPathFullyQualified(systemDirectory))
        {
            throw new InvalidOperationException(
                "Windows did not provide a valid absolute system directory.");
        }

        FileInfo windowsInstaller = new(Path.Combine(systemDirectory, "msiexec.exe"));
        if (!windowsInstaller.Exists)
        {
            throw new InvalidOperationException(
                $"Windows Installer is missing from the system directory: {windowsInstaller.FullName}");
        }
        if ((windowsInstaller.Attributes & FileAttributes.ReparsePoint) != 0)
        {
            throw new InvalidOperationException(
                $"Windows Installer must not be a reparse point: {windowsInstaller.FullName}");
        }
        return windowsInstaller.FullName;
    }

    private static void ValidateInstallRoot(string installRoot)
    {
        RejectKnownGameRuntime(installRoot);
        string current = installRoot;
        while (!string.IsNullOrWhiteSpace(current))
        {
            if (Directory.Exists(current)
                && (File.GetAttributes(current) & FileAttributes.ReparsePoint) != 0)
            {
                throw new InvalidOperationException(
                    $"The installation path contains a symbolic link or reparse point: {current}");
            }
            string? parent = Directory.GetParent(current)?.FullName;
            if (parent is null || string.Equals(parent, current, StringComparison.OrdinalIgnoreCase))
            {
                break;
            }
            current = parent;
        }
    }

    private static void RejectKnownGameRuntime(string installRoot)
    {
        if (File.Exists(Path.Combine(installRoot, "UnityPlayer.dll"))
            || Directory.Exists(Path.Combine(installRoot, "TaintedGrail_Data"))
            || File.Exists(Path.Combine(installRoot, "TaintedGrail.exe")))
        {
            throw new InvalidOperationException(
                "The installation directory appears to be a Tainted Grail or Unity runtime. Choose a separate FOA-SDK directory.");
        }
    }
}

internal static class InstalledEditorLauncher
{
    public static void Launch(string installRoot)
    {
        string launcher = Path.Combine(
            installRoot,
            "bin",
            "Windows",
            "profile",
            "Default",
            "FOA-SDK.exe");
        if (!File.Exists(launcher))
        {
            throw new InvalidOperationException(
                "FOA-SDK.exe is missing. Run Repair before starting the SDK.");
        }
        Process.Start(new ProcessStartInfo
        {
            FileName = launcher,
            UseShellExecute = true,
            WorkingDirectory = Path.GetDirectoryName(launcher)!,
        });
    }
}
