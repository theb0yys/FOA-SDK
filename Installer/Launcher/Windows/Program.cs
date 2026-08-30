// SPDX-License-Identifier: Apache-2.0 OR MIT
using System.ComponentModel;
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

            if (args.Length == 0)
            {
                int? expressResult = RunExpressInstallAsync(payload, options).GetAwaiter().GetResult();
                if (expressResult.HasValue)
                {
                    return expressResult.Value;
                }
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

    private static async Task<int?> RunExpressInstallAsync(InstallerPayload payload, InstallerOptions options)
    {
        string message = "Install the complete FOA-SDK now?"
            + Environment.NewLine + Environment.NewLine
            + "The reviewed SDK package has already been SHA-256 verified by this installer."
            + Environment.NewLine + Environment.NewLine
            + $"Install location:{Environment.NewLine}{options.InstallRoot}"
            + Environment.NewLine + Environment.NewLine
            + $"Reviewed MSI SHA-256:{Environment.NewLine}{payload.Sha256}"
            + Environment.NewLine + Environment.NewLine
            + "This installs the SDK, bundled editor project, and FOA-SDK.exe for the current Windows user. "
            + "External workspaces and Tainted Grail game files are not changed."
            + Environment.NewLine + Environment.NewLine
            + "Yes = Install now    No = Advanced options    Cancel = Exit";

        DialogResult choice = MessageBox.Show(
            message,
            Title,
            MessageBoxButtons.YesNoCancel,
            MessageBoxIcon.Information,
            MessageBoxDefaultButton.Button1);

        if (choice == DialogResult.No)
        {
            return null;
        }
        if (choice != DialogResult.Yes)
        {
            return 0;
        }

        InstallerRunResult result = await WindowsInstallerRunner.RunAsync(payload, options);
        if (!result.Succeeded)
        {
            MessageBox.Show(
                result.Message + Environment.NewLine + Environment.NewLine + $"MSI log: {result.LogPath}",
                Title,
                MessageBoxButtons.OK,
                MessageBoxIcon.Error);
            return result.ExitCode == 0 ? 1 : result.ExitCode;
        }

        MessageBox.Show(
            "FOA-SDK installation completed."
                + Environment.NewLine + Environment.NewLine
                + $"Installed to:{Environment.NewLine}{options.InstallRoot}"
                + Environment.NewLine + Environment.NewLine
                + "FOA-SDK.exe will open the installed SDK editor now.",
            Title,
            MessageBoxButtons.OK,
            MessageBoxIcon.Information);

        InstalledEditorLauncher.Launch(options.InstallRoot);
        return 0;
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

        if (options.LaunchAfterInstall && options.Operation is not InstallerOperation.Uninstall)
        {
            InstalledEditorLauncher.Launch(options.InstallRoot);
        }
        Console.WriteLine($"{result.Message} Log: {result.LogPath}");
        return 0;
    }
}
