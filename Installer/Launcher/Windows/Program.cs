// SPDX-License-Identifier: Apache-2.0 OR MIT
using System.ComponentModel;
using System.Diagnostics;
using System.Security.Cryptography;
using System.Windows.Forms;

namespace FOA.SDK.InstallerLauncher;

internal static class Program
{
    private const string Title = "FOA-SDK Installer";

    [STAThread]
    private static int Main(string[] args)
    {
        ApplicationConfiguration.Initialize();
        try
        {
            InstallerOptions options = InstallerOptions.Parse(args);

            if (options.ToolWizardOnly)
            {
                return RunToolWizard(options);
            }

            using InstallerPayload payload = InstallerPayload.Resolve(options.MsiPath);

            if (options.SmokeTest)
            {
                using InstallerWizardForm smokeForm = new(payload, options);
                smokeForm.CreateControl();
                return 0;
            }

            if (options.Quiet)
            {
                return RunQuietAsync(payload, options).GetAwaiter().GetResult();
            }

            using InstallerWizardForm form = new(payload, options);
            Application.Run(form);
            return form.ExitCode;
        }
        catch (Exception ex) when (
            ex is ArgumentException
                or IOException
                or InvalidOperationException
                or UnauthorizedAccessException
                or Win32Exception
                or CryptographicException)
        {
            if (InstallerOptions.WantsConsoleError(args))
            {
                if (string.Equals(
                    Environment.GetEnvironmentVariable("FOA_SDK_INSTALLER_DEBUG_ERRORS"),
                    "1",
                    StringComparison.Ordinal))
                {
                    Console.Error.WriteLine($"{Title}: {ex}");
                }
                else
                {
                    Console.Error.WriteLine($"{Title}: {ex.Message}");
                }
            }
            else
            {
                MessageBox.Show(ex.Message, Title, MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
            return 1;
        }
    }

    private static int RunToolWizard(InstallerOptions options)
    {
        if (options.SaveToolProfile)
        {
            ToolSetupOptions loaded = ToolSetupProfile.LoadOrDefault(options.InstallRoot);
            ToolSetupOptions setupOptions = loaded with
            {
                WorkspaceRoot = options.WorkspaceRoot ?? loaded.WorkspaceRoot,
                O3deEditorPath = options.O3deEditorPath ?? loaded.O3deEditorPath,
                UnityEditorPath = options.UnityEditorPath ?? loaded.UnityEditorPath,
                UnityProjectPath = options.UnityProjectPath ?? loaded.UnityProjectPath,
                TaintedGrailInstallPath = options.TaintedGrailInstallPath ?? loaded.TaintedGrailInstallPath,
            };
            ToolSetupSaveResult result = ToolSetupProfile.Save(setupOptions, options.InstallRoot);
            ToolSetupOptions normalized = ToolSetupProfile.Normalize(setupOptions, options.InstallRoot);
            Console.WriteLine($"Tool profile saved: {result.ProfilePath}");
            Console.WriteLine(ToolSetupProfile.Describe(normalized, result.Readiness));
            return 0;
        }

        if (options.Quiet)
        {
            throw new ArgumentException("The Tool Wizard is interactive; remove --quiet to open it.");
        }

        using ToolSetupWizardForm form = new(options.InstallRoot);
        if (options.SmokeTest)
        {
            form.CreateControl();
            return 0;
        }

        Application.Run(form);
        return form.ExitCode;
    }

    private static async Task<int> RunQuietAsync(InstallerPayload payload, InstallerOptions options)
    {
        InstallerRunResult result = await WindowsInstallerRunner.RunAsync(payload, options);
        if (!result.Succeeded)
        {
            Console.Error.WriteLine($"{Title}: {result.Message} Log: {result.LogPath}");
            return result.ExitCode == 0 ? 1 : result.ExitCode;
        }

        if (options.Operation is not InstallerOperation.Uninstall)
        {
            InstalledEditorValidationResult validation = await InstalledEditorLauncher.ValidateAsync(options.InstallRoot);
            if (!validation.Succeeded)
            {
                Console.Error.WriteLine($"{Title}: {validation.Message} Log: {result.LogPath}");
                return validation.ExitCode == 0 ? 1 : validation.ExitCode;
            }
        }

        if (options.OpenControlPanelAfterInstall && options.Operation is not InstallerOperation.Uninstall)
        {
            InstalledEditorLauncher.LaunchControlPanel(options.InstallRoot);
        }
        if (options.LaunchAfterInstall && options.Operation is not InstallerOperation.Uninstall)
        {
            InstalledEditorLauncher.Launch(options.InstallRoot);
        }
        if (options.OpenToolWizardAfterInstall && options.Operation is not InstallerOperation.Uninstall)
        {
            LaunchToolWizardProcess(options.InstallRoot);
        }
        Console.WriteLine($"{result.Message} Log: {result.LogPath}");
        return 0;
    }

    private static void LaunchToolWizardProcess(string installRoot)
    {
        string executable = Environment.ProcessPath
            ?? throw new InvalidOperationException("The installer executable path is unavailable.");
        ProcessStartInfo startInfo = new()
        {
            FileName = executable,
            WorkingDirectory = AppContext.BaseDirectory,
            UseShellExecute = false,
        };
        startInfo.ArgumentList.Add("--tool-wizard");
        startInfo.ArgumentList.Add("--install-root");
        startInfo.ArgumentList.Add(installRoot);
        _ = System.Diagnostics.Process.Start(startInfo)
            ?? throw new InvalidOperationException("The legacy Tool Setup Wizard did not start.");
    }
}
