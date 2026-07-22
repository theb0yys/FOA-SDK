// SPDX-License-Identifier: Apache-2.0 OR MIT
using System.Drawing;
using System.Windows.Forms;

namespace FOA.SDK.InstallerLauncher;

internal sealed class InstallerWizardForm : Form
{
    private readonly InstallerPayload _payload;
    private InstallerOptions _options;
    private readonly Panel _content = new() { Dock = DockStyle.Fill, Padding = new Padding(28) };
    private readonly Button _backButton = new() { Text = "< Back", Width = 92, Height = 32 };
    private readonly Button _nextButton = new() { Text = "Next >", Width = 92, Height = 32 };
    private readonly Button _cancelButton = new() { Text = "Cancel", Width = 92, Height = 32 };
    private readonly RadioButton _installRadio = new() { Text = "Install or upgrade the complete FOA-SDK", AutoSize = true, Checked = true };
    private readonly RadioButton _repairRadio = new() { Text = "Repair the installed FOA-SDK", AutoSize = true };
    private readonly RadioButton _uninstallRadio = new() { Text = "Uninstall FOA-SDK", AutoSize = true };
    private readonly TextBox _installRoot = new() { Width = 460 };
    private readonly Label _reviewText = NewBodyLabel();
    private readonly Label _resultText = NewBodyLabel();
    private readonly CheckBox _launchEditor = new() { Text = "Launch Tainted Grail Modding Editor", AutoSize = true, Checked = true };
    private readonly ProgressBar _progress = new() { Style = ProgressBarStyle.Marquee, MarqueeAnimationSpeed = 30, Width = 520, Height = 22 };
    private readonly List<Control> _pages = new();
    private int _pageIndex;
    private bool _operationRunning;
    private bool _operationSucceeded;

    public InstallerWizardForm(InstallerPayload payload, InstallerOptions options)
    {
        _payload = payload;
        _options = options;
        ExitCode = 1;
        Text = "FOA-SDK Setup";
        StartPosition = FormStartPosition.CenterScreen;
        MinimumSize = new Size(720, 500);
        Size = new Size(760, 540);
        FormBorderStyle = FormBorderStyle.FixedDialog;
        MaximizeBox = false;
        MinimizeBox = true;
        AutoScaleMode = AutoScaleMode.Dpi;

        Controls.Add(_content);
        Controls.Add(BuildButtonBar());
        _installRoot.Text = options.InstallRoot;
        ApplyInitialOperation(options.Operation);
        _pages.Add(BuildWelcomePage());
        _pages.Add(BuildOptionsPage());
        _pages.Add(BuildReviewPage());
        _pages.Add(BuildProgressPage());
        _pages.Add(BuildResultPage());

        _backButton.Click += (_, _) => SetPage(_pageIndex - 1);
        _nextButton.Click += NextClicked;
        _cancelButton.Click += (_, _) => Close();
        FormClosing += (_, eventArgs) =>
        {
            if (_operationRunning)
            {
                eventArgs.Cancel = true;
            }
        };
        SetPage(0);
    }

    public int ExitCode { get; private set; }

    private Control BuildButtonBar()
    {
        FlowLayoutPanel bar = new()
        {
            Dock = DockStyle.Bottom,
            Height = 58,
            FlowDirection = FlowDirection.RightToLeft,
            Padding = new Padding(12),
            BackColor = SystemColors.ControlLight,
        };
        bar.Controls.Add(_cancelButton);
        bar.Controls.Add(_nextButton);
        bar.Controls.Add(_backButton);
        AcceptButton = _nextButton;
        CancelButton = _cancelButton;
        return bar;
    }

    private Control BuildWelcomePage()
    {
        return BuildPage(
            "Welcome to FOA-SDK Setup",
            "This wizard installs the complete prebuilt Tainted Grail: The Fall of Avalon Modding Editor and SDK.\n\n"
            + "No Git, Python, CMake, Visual Studio, or source build is required. The reviewed MSI is embedded in this application and its SHA-256 is verified before Windows Installer starts.\n\n"
            + "Keep workspaces and generated mod output outside the installation directory.");
    }

    private Control BuildOptionsPage()
    {
        Panel page = BuildPage(
            "Choose an operation",
            "Install the complete SDK, repair product-owned files, or remove the installed product. External workspaces are never removed.");
        FlowLayoutPanel choices = new()
        {
            FlowDirection = FlowDirection.TopDown,
            AutoSize = true,
            Location = new Point(32, 150),
        };
        choices.Controls.Add(_installRadio);
        choices.Controls.Add(_repairRadio);
        choices.Controls.Add(_uninstallRadio);

        Label locationLabel = new() { Text = "Installation directory:", AutoSize = true, Margin = new Padding(0, 18, 0, 4) };
        FlowLayoutPanel locationRow = new() { FlowDirection = FlowDirection.LeftToRight, AutoSize = true };
        Button browse = new() { Text = "Browse...", Width = 88 };
        browse.Click += (_, _) => BrowseInstallRoot();
        locationRow.Controls.Add(_installRoot);
        locationRow.Controls.Add(browse);
        choices.Controls.Add(locationLabel);
        choices.Controls.Add(locationRow);
        page.Controls.Add(choices);
        return page;
    }

    private Control BuildReviewPage()
    {
        Panel page = BuildPage("Ready to continue", string.Empty);
        _reviewText.Location = new Point(32, 115);
        _reviewText.Size = new Size(640, 260);
        page.Controls.Add(_reviewText);
        return page;
    }

    private Control BuildProgressPage()
    {
        Panel page = BuildPage(
            "Applying FOA-SDK changes",
            "Windows Installer is processing the reviewed package. Do not close this window.");
        _progress.Location = new Point(32, 170);
        page.Controls.Add(_progress);
        return page;
    }

    private Control BuildResultPage()
    {
        Panel page = BuildPage("Setup result", string.Empty);
        _resultText.Location = new Point(32, 115);
        _resultText.Size = new Size(640, 220);
        _launchEditor.Location = new Point(32, 350);
        page.Controls.Add(_resultText);
        page.Controls.Add(_launchEditor);
        return page;
    }

    private static Panel BuildPage(string heading, string body)
    {
        Panel page = new() { Dock = DockStyle.Fill };
        Font baseFont = SystemFonts.MessageBoxFont ?? SystemFonts.DefaultFont;
        Label title = new()
        {
            Text = heading,
            Font = new Font(baseFont.FontFamily, 18, FontStyle.Bold),
            AutoSize = true,
            Location = new Point(28, 25),
        };
        Label text = NewBodyLabel();
        text.Text = body;
        text.Location = new Point(32, 88);
        text.Size = new Size(640, 300);
        page.Controls.Add(title);
        page.Controls.Add(text);
        return page;
    }

    private static Label NewBodyLabel() => new()
    {
        AutoSize = false,
        Font = SystemFonts.MessageBoxFont,
        UseMnemonic = false,
    };

    private void ApplyInitialOperation(InstallerOperation operation)
    {
        _installRadio.Checked = operation is InstallerOperation.InstallOrUpgrade;
        _repairRadio.Checked = operation is InstallerOperation.Repair;
        _uninstallRadio.Checked = operation is InstallerOperation.Uninstall;
    }

    private void BrowseInstallRoot()
    {
        using FolderBrowserDialog dialog = new()
        {
            Description = "Choose the per-user FOA-SDK installation directory",
            InitialDirectory = Directory.Exists(_installRoot.Text)
                ? _installRoot.Text
                : Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            ShowNewFolderButton = true,
        };
        if (dialog.ShowDialog(this) == DialogResult.OK)
        {
            _installRoot.Text = dialog.SelectedPath;
        }
    }

    private void NextClicked(object? sender, EventArgs eventArgs)
    {
        if (_pageIndex == 0)
        {
            SetPage(1);
            return;
        }
        if (_pageIndex == 1)
        {
            try
            {
                CaptureOptions();
                UpdateReview();
                SetPage(2);
            }
            catch (ArgumentException ex)
            {
                MessageBox.Show(this, ex.Message, "Invalid installation directory", MessageBoxButtons.OK, MessageBoxIcon.Warning);
            }
            return;
        }
        if (_pageIndex == 2)
        {
            _ = RunOperationAsync();
            return;
        }
        if (_pageIndex == 4)
        {
            if (_operationSucceeded && _launchEditor.Visible && _launchEditor.Checked)
            {
                try
                {
                    InstalledEditorLauncher.Launch(_options.InstallRoot);
                }
                catch (InvalidOperationException ex)
                {
                    MessageBox.Show(this, ex.Message, "Unable to launch Editor", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    ExitCode = 1;
                    return;
                }
            }
            Close();
        }
    }

    private void CaptureOptions()
    {
        InstallerOperation operation = _repairRadio.Checked
            ? InstallerOperation.Repair
            : _uninstallRadio.Checked
                ? InstallerOperation.Uninstall
                : InstallerOperation.InstallOrUpgrade;
        _options = _options with
        {
            InstallRoot = InstallerOptions.NormalizeInstallRoot(_installRoot.Text),
            Operation = operation,
        };
    }

    private void UpdateReview()
    {
        string operation = _options.Operation switch
        {
            InstallerOperation.InstallOrUpgrade => "Install or upgrade the complete prebuilt FOA-SDK",
            InstallerOperation.Repair => "Repair all product-owned FOA-SDK files",
            InstallerOperation.Uninstall => "Remove product-owned FOA-SDK files",
            _ => throw new InvalidOperationException("Unsupported installer operation."),
        };
        _reviewText.Text = $"Operation: {operation}\n\n"
            + $"Installation directory: {_options.InstallRoot}\n\n"
            + $"Reviewed MSI SHA-256: {_payload.Sha256}\n\n"
            + "External workspaces, generated content, FoA diagnostics, and game files are outside this operation.";
    }

    private async Task RunOperationAsync()
    {
        _operationRunning = true;
        SetPage(3);
        try
        {
            InstallerRunResult result = await WindowsInstallerRunner.RunAsync(_payload, _options);
            _operationSucceeded = result.Succeeded;
            ExitCode = result.Succeeded ? 0 : result.ExitCode == 0 ? 1 : result.ExitCode;
            _resultText.Text = $"{result.Message}\n\nLog: {result.LogPath}";
            _launchEditor.Visible = result.Succeeded && _options.Operation is not InstallerOperation.Uninstall;
            _launchEditor.Checked = true;
        }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException or InvalidOperationException or System.ComponentModel.Win32Exception)
        {
            _operationSucceeded = false;
            ExitCode = 1;
            _resultText.Text = $"Setup failed before completion.\n\n{ex.Message}";
            _launchEditor.Visible = false;
        }
        finally
        {
            _operationRunning = false;
            SetPage(4);
        }
    }

    private void SetPage(int index)
    {
        if (index < 0 || index >= _pages.Count)
        {
            return;
        }
        _pageIndex = index;
        _content.Controls.Clear();
        _content.Controls.Add(_pages[index]);
        _backButton.Enabled = !_operationRunning && index is 1 or 2;
        _cancelButton.Enabled = !_operationRunning && index != 4;
        _nextButton.Enabled = !_operationRunning && index != 3;
        _nextButton.Text = index switch
        {
            2 => "Apply",
            4 => "Finish",
            _ => "Next >",
        };
    }
}
