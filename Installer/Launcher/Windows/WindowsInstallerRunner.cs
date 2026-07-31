// SPDX-License-Identifier: Apache-2.0 OR MIT
using System.Diagnostics;
using System.Text;

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
        EnsureDirectoryExists(logDirectory);
        string logFileName = $"{DateTime.UtcNow:yyyyMMddTHHmmssZ}-{options.Operation.ToString().ToLowerInvariant()}-{Guid.NewGuid():N}.log";
        string logPath = Path.Combine(logDirectory, logFileName);
        string? evidenceLogPath = options.EvidenceRoot is null
            ? null
            : Path.Combine(options.EvidenceRoot, "installer-logs", logFileName);

        ProcessStartInfo startInfo = new()
        {
            FileName = windowsInstallerPath,
            UseShellExecute = false,
            CreateNoWindow = true,
            WorkingDirectory = payload.MsiFile.DirectoryName ?? AppContext.BaseDirectory,
            Arguments = BuildWindowsInstallerArguments(payload, options, logPath),
        };
        if (IsDebugConsoleEnabled())
        {
            Console.Error.WriteLine($"Windows Installer path: {startInfo.FileName}");
            Console.Error.WriteLine($"Windows Installer working directory: {startInfo.WorkingDirectory}");
            Console.Error.WriteLine($"Windows Installer arguments: {startInfo.Arguments}");
        }

        using Process process = Process.Start(startInfo)
            ?? throw new InvalidOperationException("Windows Installer did not start.");
        await process.WaitForExitAsync();

        int exitCode = process.ExitCode;
        bool succeeded = SuccessfulExitCodes.Contains(exitCode);
        bool rebootRequired = exitCode is 1641 or 3010;
        string reportedLogPath = CopyLogToEvidenceIfRequested(logPath, evidenceLogPath);
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
            : $"{operation} failed with Windows Installer exit code {exitCode}: {DescribeExitCode(exitCode)}";
        return new InstallerRunResult(succeeded, exitCode, rebootRequired, message, reportedLogPath);
    }

    private static string BuildWindowsInstallerArguments(
        InstallerPayload payload,
        InstallerOptions options,
        string logPath)
    {
        string verb = options.Operation switch
        {
            InstallerOperation.InstallOrUpgrade => "/i",
            InstallerOperation.Repair => "/fvamus",
            InstallerOperation.Uninstall => "/x",
            _ => throw new InvalidOperationException("Unsupported installer operation."),
        };
        return string.Join(
            " ",
            verb,
            QuoteProcessArgument(payload.MsiFile.FullName),
            "/qn",
            "/norestart",
            FormatInstallerPropertyArgument("INSTALL_ROOT", options.InstallRoot),
            "/l*v",
            QuoteProcessArgument(logPath));
    }

    private static string FormatInstallerPropertyArgument(string propertyName, string value)
    {
        if (string.IsNullOrWhiteSpace(propertyName)
            || !propertyName.All(character =>
                character == '_'
                || character is >= 'A' and <= 'Z'
                || character is >= '0' and <= '9'))
        {
            throw new InvalidOperationException("Windows Installer property names must be uppercase public properties.");
        }
        if (value.Contains('"'))
        {
            throw new InvalidOperationException("Windows Installer property values must not contain quotation marks.");
        }
        return $"{propertyName}={QuoteProcessArgument(value)}";
    }

    private static string QuoteProcessArgument(string value)
    {
        if (value.Contains('"'))
        {
            throw new InvalidOperationException("Windows Installer command arguments must not contain quotation marks.");
        }

        StringBuilder builder = new(value.Length + 2);
        builder.Append('"');
        int trailingBackslashes = 0;
        foreach (char character in value)
        {
            if (character == '\\')
            {
                trailingBackslashes++;
                builder.Append(character);
                continue;
            }
            trailingBackslashes = 0;
            builder.Append(character);
        }
        builder.Insert(builder.Length - trailingBackslashes, new string('\\', trailingBackslashes));
        builder.Append('"');
        return builder.ToString();
    }

    private static string DescribeExitCode(int exitCode) => exitCode switch
    {
        1602 => "the installation was cancelled.",
        1603 => "Windows Installer reported a fatal installation error.",
        1618 => "another Windows Installer operation is already in progress.",
        1638 => "another version of this product is already installed.",
        1639 => "Windows Installer rejected the command line before it could apply the package.",
        _ => "Windows Installer did not complete the requested operation.",
    };

    private static bool IsDebugConsoleEnabled() => string.Equals(
        Environment.GetEnvironmentVariable("FOA_SDK_INSTALLER_DEBUG_ERRORS"),
        "1",
        StringComparison.Ordinal);

    private static string CopyLogToEvidenceIfRequested(string sourceLogPath, string? evidenceLogPath)
    {
        if (string.IsNullOrWhiteSpace(evidenceLogPath))
        {
            return sourceLogPath;
        }

        try
        {
            string? evidenceDirectory = Path.GetDirectoryName(evidenceLogPath);
            if (string.IsNullOrWhiteSpace(evidenceDirectory))
            {
                return sourceLogPath;
            }
            EnsureDirectoryExists(evidenceDirectory);
            if (File.Exists(sourceLogPath))
            {
                File.Copy(sourceLogPath, evidenceLogPath, overwrite: true);
                return evidenceLogPath;
            }
        }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException or ArgumentException or NotSupportedException)
        {
            if (IsDebugConsoleEnabled())
            {
                Console.Error.WriteLine($"Evidence log copy failed: {ex.Message}");
            }
        }
        return sourceLogPath;
    }

    private static void EnsureDirectoryExists(string directory)
    {
        string fullPath = Path.GetFullPath(directory);
        Stack<string> missingDirectories = new();
        string? current = fullPath;
        while (!string.IsNullOrWhiteSpace(current) && !Directory.Exists(current))
        {
            missingDirectories.Push(current);
            string? parent = Directory.GetParent(current)?.FullName;
            if (string.IsNullOrWhiteSpace(parent)
                || string.Equals(parent, current, StringComparison.OrdinalIgnoreCase))
            {
                break;
            }
            current = parent;
        }

        while (missingDirectories.TryPop(out string? missingDirectory))
        {
            if (missingDirectory is not null)
            {
                Directory.CreateDirectory(missingDirectory);
            }
        }
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
