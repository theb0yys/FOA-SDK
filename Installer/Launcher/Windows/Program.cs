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
                Console.Error.WriteLine($"{Title}: {ex.Message}");
            }
            else
            {
                MessageBox.Show(ex.Message, Title, MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
            return 1;
        }
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
