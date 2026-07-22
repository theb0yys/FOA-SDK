// SPDX-License-Identifier: Apache-2.0 OR MIT
using System.ComponentModel;
using System.Diagnostics;
using System.Windows.Forms;

namespace FOA.SDK.InstallerLauncher;

internal static class Program
{
    private const string Title = "FOA-SDK Installer";
    private static readonly string[] PythonCandidates =
    {
        "pythonw.exe",
        "python.exe",
        "py.exe",
        "python3.exe",
        "python"
    };

    [STAThread]
    private static int Main(string[] args)
    {
        try
        {
            LaunchOptions options = LaunchOptions.Parse(args);
            ApplicationConfiguration.Initialize();
            PathBundle paths = PathBundle.Resolve(options);
            string python = ResolvePython(options, paths);
            ProcessStartInfo startInfo = BuildStartInfo(options, paths, python);
            using Process process = Process.Start(startInfo)
                ?? throw new InvalidOperationException("The FOA-SDK reviewed installer process did not start.");
            if (options.Detach)
            {
                return 0;
            }
            process.WaitForExit();
            return process.ExitCode;
        }
        catch (Exception ex) when (ex is ArgumentException or IOException or InvalidOperationException or Win32Exception)
        {
            if (LaunchOptions.WantsConsoleError(args))
            {
                Console.Error.WriteLine($"{Title}: {ex.Message}");
            }
            else
            {
                ApplicationConfiguration.Initialize();
                MessageBox.Show(ex.Message, Title, MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
            return 1;
        }
    }

    private static ProcessStartInfo BuildStartInfo(LaunchOptions options, PathBundle paths, string python)
    {
        string installerRoot = paths.InstallerRoot.FullName;
        ProcessStartInfo info = new()
        {
            FileName = python,
            WorkingDirectory = paths.ProductRoot.FullName,
            UseShellExecute = false,
            CreateNoWindow = !options.SmokeTest,
        };
        info.ArgumentList.Add(paths.ReviewHost.FullName);
        info.ArgumentList.Add("--installer-root");
        info.ArgumentList.Add(installerRoot);
        if (options.SmokeTest)
        {
            info.ArgumentList.Add("--smoke-test");
        }
        info.Environment["FOA_SDK_INSTALLER_ROOT"] = installerRoot;
        info.Environment["FOA_SDK_PRODUCT_ROOT"] = paths.ProductRoot.FullName;
        return info;
    }

    private static string ResolvePython(LaunchOptions options, PathBundle paths)
    {
        if (!string.IsNullOrWhiteSpace(options.PythonPath))
        {
            return RequireExistingFile(options.PythonPath!, "Configured Python executable").FullName;
        }
        string? envPython = Environment.GetEnvironmentVariable("FOA_SDK_PYTHON");
        if (!string.IsNullOrWhiteSpace(envPython))
        {
            return RequireExistingFile(envPython!, "FOA_SDK_PYTHON").FullName;
        }
        foreach (FileInfo bundled in paths.BundledPythonCandidates())
        {
            if (bundled.Exists)
            {
                return bundled.FullName;
            }
        }
        foreach (string candidate in PythonCandidates)
        {
            if (CommandExists(candidate))
            {
                return candidate;
            }
        }
        throw new InvalidOperationException(
            "Python could not be found. Install Python with tkinter support, set FOA_SDK_PYTHON, " +
            "or place pythonw.exe beside the launcher under runtime\\python\\pythonw.exe."
        );
    }

    private static bool CommandExists(string command)
    {
        string pathVariable = Environment.GetEnvironmentVariable("PATH") ?? string.Empty;
        foreach (string raw in pathVariable.Split(Path.PathSeparator, StringSplitOptions.RemoveEmptyEntries))
        {
            try
            {
                string candidate = Path.Combine(raw.Trim('"'), command);
                if (File.Exists(candidate))
                {
                    return true;
                }
            }
            catch (ArgumentException)
            {
                continue;
            }
        }
        return false;
    }

    private static FileInfo RequireExistingFile(string path, string label)
    {
        FileInfo file = new(Path.GetFullPath(Environment.ExpandEnvironmentVariables(path)));
        if (!file.Exists)
        {
            throw new InvalidOperationException($"{label} does not exist: {file.FullName}");
        }
        return file;
    }

    private sealed record LaunchOptions(
        DirectoryInfo? InstallerRoot,
        string? PythonPath,
        bool SmokeTest,
        bool Detach,
        bool NoDialog
    )
    {
        public static LaunchOptions Parse(string[] args)
        {
            DirectoryInfo? installerRoot = null;
            string? pythonPath = null;
            bool smokeTest = false;
            bool detach = false;
            bool noDialog = false;
            for (int index = 0; index < args.Length; index++)
            {
                string current = args[index];
                switch (current)
                {
                    case "--installer-root":
                        installerRoot = new DirectoryInfo(RequireValue(args, ref index, current));
                        break;
                    case "--python":
                        pythonPath = RequireValue(args, ref index, current);
                        break;
                    case "--smoke-test":
                        smokeTest = true;
                        noDialog = true;
                        break;
                    case "--detach":
                        detach = true;
                        break;
                    case "--no-dialog":
                        noDialog = true;
                        break;
                    case "--advanced-review":
                        // Retained as a compatibility no-op: reviewed mode is now the only launcher mode.
                        break;
                    case "--help":
                    case "-h":
                        throw new ArgumentException(HelpText());
                    default:
                        throw new ArgumentException($"Unknown option: {current}\n\n{HelpText()}");
                }
            }
            return new LaunchOptions(installerRoot, pythonPath, smokeTest, detach, noDialog);
        }

        public static bool WantsConsoleError(string[] args)
        {
            return args.Contains("--smoke-test", StringComparer.Ordinal) || args.Contains("--no-dialog", StringComparer.Ordinal);
        }

        private static string RequireValue(string[] args, ref int index, string option)
        {
            if (index + 1 >= args.Length || args[index + 1].StartsWith("--", StringComparison.Ordinal))
            {
                throw new ArgumentException($"{option} requires a value.\n\n{HelpText()}");
            }
            index++;
            return args[index];
        }

        private static string HelpText()
        {
            return "Usage: FOA-SDK-Installer.exe [--installer-root <Installer>] [--python <pythonw.exe>] [--smoke-test] [--detach] [--no-dialog]";
        }
    }

    private sealed record PathBundle(DirectoryInfo ProductRoot, DirectoryInfo InstallerRoot, FileInfo ReviewHost)
    {
        public static PathBundle Resolve(LaunchOptions options)
        {
            DirectoryInfo installer = options.InstallerRoot is not null
                ? ExistingDirectory(options.InstallerRoot.FullName, "Installer root")
                : DiscoverInstallerRoot();
            FileInfo reviewHost = new(Path.Combine(installer.FullName, "SuiteWizard", "Host", "Source", "suite_wizard_receipt_host.py"));
            if (!reviewHost.Exists)
            {
                throw new InvalidOperationException($"Reviewed Suite Wizard host was not found: {reviewHost.FullName}");
            }
            DirectoryInfo productRoot = installer.Parent ?? throw new InvalidOperationException("Installer root has no product root.");
            return new PathBundle(productRoot, installer, reviewHost);
        }

        public IEnumerable<FileInfo> BundledPythonCandidates()
        {
            yield return new FileInfo(Path.Combine(AppContext.BaseDirectory, "runtime", "python", "pythonw.exe"));
            yield return new FileInfo(Path.Combine(AppContext.BaseDirectory, "python", "pythonw.exe"));
            yield return new FileInfo(Path.Combine(ProductRoot.FullName, "runtime", "python", "pythonw.exe"));
        }

        private static DirectoryInfo DiscoverInstallerRoot()
        {
            IEnumerable<DirectoryInfo> seeds = new[]
            {
                new DirectoryInfo(AppContext.BaseDirectory),
                new DirectoryInfo(Environment.CurrentDirectory)
            };
            foreach (DirectoryInfo seed in seeds)
            {
                DirectoryInfo? current = seed;
                while (current is not null)
                {
                    DirectoryInfo candidate = new(Path.Combine(current.FullName, "Installer"));
                    FileInfo reviewHost = new(Path.Combine(candidate.FullName, "SuiteWizard", "Host", "Source", "suite_wizard_receipt_host.py"));
                    if (candidate.Exists && reviewHost.Exists)
                    {
                        return candidate;
                    }
                    current = current.Parent;
                }
            }
            throw new InvalidOperationException(
                "Unable to locate the Installer directory. Start the launcher from the product checkout " +
                "or pass --installer-root <path-to-Installer>."
            );
        }

        private static DirectoryInfo ExistingDirectory(string path, string label)
        {
            DirectoryInfo directory = new(Path.GetFullPath(Environment.ExpandEnvironmentVariables(path)));
            if (!directory.Exists)
            {
                throw new InvalidOperationException($"{label} does not exist: {directory.FullName}");
            }
            return directory;
        }
    }
}
