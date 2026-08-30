// SPDX-License-Identifier: Apache-2.0 OR MIT
using System.Drawing;
using System.Windows.Forms;

namespace FOA.SDK.InstallerLauncher;

internal sealed class InstallerWizardForm : Form
{
    private static readonly Color WindowBackground = Color.FromArgb(245, 247, 246);
    private static readonly Color HeaderBackground = Color.FromArgb(31, 43, 40);
    private static readonly Color Accent = Color.FromArgb(211, 164, 67);
    private static readonly Color Ink = Color.FromArgb(28, 35, 33);
    private static readonly Color Muted = Color.FromArgb(94, 104, 100);
    private static readonly Color Border = Color.FromArgb(211, 218, 215);
    private static readonly Color Success = Color.FromArgb(34, 112, 74);
    private static readonly Color Error = Color.FromArgb(164, 54, 45);
    private static readonly Font BaseFont = new(
        (SystemFonts.MessageBoxFont ?? SystemFonts.DefaultFont).FontFamily,
        9f,
        FontStyle.Regular);

    private readonly InstallerPayload _payload;
    private InstallerOptions _options;

    private readonly Panel _content = new() { Dock = DockStyle.Fill, BackColor = WindowBackground };
    private readonly Button _primaryButton = NewPrimaryButton("Install");
    private readonly Button _cancelButton = NewSecondaryButton("Cancel");
    private readonly TextBox _installRoot = new()
    {
        BorderStyle = BorderStyle.FixedSingle,
        Font = BaseFont,
        Height = 30,
    };
    private readonly ProgressBar _progress = new()
    {
        Style = ProgressBarStyle.Marquee,
        MarqueeAnimationSpeed = 24,
        Height = 18,
    };
    private readonly Label _progressTitle = NewTitleLabel();
    private readonly Label _progressText = NewBodyLabel();
    private readonly Label _resultTitle = NewTitleLabel();
    private readonly Label _resultText = NewBodyLabel();
    private readonly CheckBox _launchEditor = new()
    {
        Text = "Open FOA-SDK",
        AutoSize = true,
        Checked = true,
        Font = BaseFont,
        ForeColor = Ink,
    };
    private readonly CheckBox _createDesktopShortcut = new()
    {
        Text = "Create desktop shortcut",
        AutoSize = true,
        Checked = true,
        Font = BaseFont,
        ForeColor = Ink,
    };

    private Control? _installPage;
    private Control? _progressPage;
    private Control? _resultPage;
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
        Size = new Size(780, 580);
        MinimumSize = new Size(780, 580);
        MaximumSize = new Size(780, 580);
        FormBorderStyle = FormBorderStyle.FixedDialog;
        MaximizeBox = false;
        MinimizeBox = true;
        AutoScaleMode = AutoScaleMode.Dpi;
        Font = BaseFont;
        BackColor = WindowBackground;

        _installRoot.Text = options.InstallRoot;
        _installPage = BuildInstallPage();
        _progressPage = BuildProgressPage();
        _resultPage = BuildResultPage();

        Controls.Add(BuildRootLayout());
        _primaryButton.Click += PrimaryClicked;
        _cancelButton.Click += (_, _) => Close();
        FormClosing += (_, eventArgs) =>
        {
            if (_operationRunning)
            {
                eventArgs.Cancel = true;
            }
        };

        ShowInstallPage();
    }

    public int ExitCode { get; private set; }

    private Control BuildRootLayout()
    {
        TableLayoutPanel root = new()
        {
            Dock = DockStyle.Fill,
            ColumnCount = 1,
            RowCount = 3,
            BackColor = WindowBackground,
        };
        root.RowStyles.Add(new RowStyle(SizeType.Absolute, 92));
        root.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
        root.RowStyles.Add(new RowStyle(SizeType.Absolute, 78));
        root.Controls.Add(BuildHeader(), 0, 0);
        root.Controls.Add(_content, 0, 1);
        root.Controls.Add(BuildFooter(), 0, 2);
        return root;
    }

    private static Control BuildHeader()
    {
        Panel header = new()
        {
            Dock = DockStyle.Fill,
            BackColor = HeaderBackground,
            Padding = new Padding(34, 18, 34, 14),
        };

        Label product = new()
        {
            Text = "FOA-SDK",
            AutoSize = true,
            Font = new Font(BaseFont.FontFamily, 14f, FontStyle.Bold),
            ForeColor = Color.White,
            Location = new Point(34, 18),
        };
        Label setup = new()
        {
            Text = "Setup",
            AutoSize = true,
            Font = new Font(BaseFont.FontFamily, 9f, FontStyle.Regular),
            ForeColor = Color.FromArgb(188, 202, 196),
            Location = new Point(36, 54),
        };
        Panel accent = new()
        {
            BackColor = Accent,
            Size = new Size(6, 52),
            Location = new Point(18, 20),
        };
        header.Controls.Add(accent);
        header.Controls.Add(product);
        header.Controls.Add(setup);
        return header;
    }

    private Control BuildFooter()
    {
        FlowLayoutPanel footer = new()
        {
            Dock = DockStyle.Fill,
            FlowDirection = FlowDirection.RightToLeft,
            WrapContents = false,
            Padding = new Padding(22, 18, 30, 14),
            BackColor = Color.White,
        };
        footer.Paint += (_, eventArgs) =>
        {
            using Pen pen = new(Border);
            eventArgs.Graphics.DrawLine(pen, 0, 0, footer.Width, 0);
        };
        footer.Controls.Add(_primaryButton);
        footer.Controls.Add(_cancelButton);
        AcceptButton = _primaryButton;
        CancelButton = _cancelButton;
        return footer;
    }

    private Control BuildInstallPage()
    {
        Panel page = NewPage();
        Label title = NewTitleLabel();
        title.Text = OperationTitle();
        title.Location = new Point(42, 36);
        title.Size = new Size(660, 42);

        Label body = NewBodyLabel();
        body.Text = _options.Operation switch
        {
            InstallerOperation.Repair => "Choose the FOA-SDK installation to repair.",
            InstallerOperation.Uninstall => "Choose the FOA-SDK installation to remove.",
            _ => "Choose where FOA-SDK should be installed.",
        };
        body.Location = new Point(42, 86);
        body.Size = new Size(660, 34);

        Panel locationCard = new()
        {
            Location = new Point(42, 148),
            Size = new Size(660, 138),
            BackColor = Color.White,
            BorderStyle = BorderStyle.FixedSingle,
        };
        Label locationLabel = new()
        {
            Text = "Install location",
            AutoSize = true,
            Font = new Font(BaseFont, FontStyle.Bold),
            ForeColor = Ink,
            Location = new Point(20, 18),
        };
        _installRoot.Location = new Point(20, 58);
        _installRoot.Size = new Size(500, 30);
        Button browseButton = NewSecondaryButton("Browse");
        browseButton.Location = new Point(532, 56);
        browseButton.Size = new Size(104, 34);
        browseButton.Click += (_, _) => BrowseInstallRoot();

        locationCard.Controls.Add(locationLabel);
        locationCard.Controls.Add(_installRoot);
        locationCard.Controls.Add(browseButton);
        page.Controls.Add(title);
        page.Controls.Add(body);
        page.Controls.Add(locationCard);
        return page;
    }

    private Control BuildProgressPage()
    {
        Panel page = NewPage();
        _progressTitle.Location = new Point(42, 64);
        _progressTitle.Size = new Size(660, 46);
        _progressText.Location = new Point(42, 120);
        _progressText.Size = new Size(660, 54);
        _progress.Location = new Point(42, 214);
        _progress.Size = new Size(660, 18);
        page.Controls.Add(_progressTitle);
        page.Controls.Add(_progressText);
        page.Controls.Add(_progress);
        return page;
    }

    private Control BuildResultPage()
    {
        Panel page = NewPage();
        _resultTitle.Location = new Point(42, 46);
        _resultTitle.Size = new Size(660, 46);
        _resultText.Location = new Point(42, 104);
        _resultText.Size = new Size(660, 112);

        Panel choices = new()
        {
            Location = new Point(42, 244),
            Size = new Size(660, 108),
            BackColor = Color.White,
            BorderStyle = BorderStyle.FixedSingle,
        };
        _launchEditor.Location = new Point(22, 22);
        _createDesktopShortcut.Location = new Point(22, 62);
        choices.Controls.Add(_launchEditor);
        choices.Controls.Add(_createDesktopShortcut);
        choices.Name = "finish-options";

        page.Controls.Add(_resultTitle);
        page.Controls.Add(_resultText);
        page.Controls.Add(choices);
        return page;
    }

    private static Panel NewPage() => new()
    {
        Dock = DockStyle.Fill,
        BackColor = WindowBackground,
    };

    private static Label NewTitleLabel() => new()
    {
        AutoSize = false,
        Font = new Font(BaseFont.FontFamily, 15f, FontStyle.Bold),
        ForeColor = Ink,
    };

    private static Label NewBodyLabel() => new()
    {
        AutoSize = false,
        Font = BaseFont,
        ForeColor = Muted,
    };

    private static Button NewPrimaryButton(string text) => new()
    {
        Text = text,
        Width = 118,
        Height = 38,
        FlatStyle = FlatStyle.System,
        Margin = new Padding(8, 0, 0, 0),
    };

    private static Button NewSecondaryButton(string text) => new()
    {
        Text = text,
        Width = 118,
        Height = 38,
        FlatStyle = FlatStyle.System,
        Margin = new Padding(8, 0, 0, 0),
    };

    private string OperationTitle() => _options.Operation switch
    {
        InstallerOperation.Repair => "Repair FOA-SDK",
        InstallerOperation.Uninstall => "Uninstall FOA-SDK",
        _ => "Install FOA-SDK",
    };

    private string OperationButtonText() => _options.Operation switch
    {
        InstallerOperation.Repair => "Repair",
        InstallerOperation.Uninstall => "Uninstall",
        _ => "Install",
    };

    private void BrowseInstallRoot()
    {
        string fallback = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
        using FolderBrowserDialog dialog = new()
        {
            Description = "Choose the folder where FOA-SDK should be installed.",
            InitialDirectory = Directory.Exists(_installRoot.Text) ? _installRoot.Text : fallback,
            SelectedPath = Directory.Exists(_installRoot.Text) ? _installRoot.Text : fallback,
            ShowNewFolderButton = true,
            UseDescriptionForTitle = true,
        };
        if (dialog.ShowDialog(this) == DialogResult.OK)
        {
            _installRoot.Text = dialog.SelectedPath;
        }
    }

    private async void PrimaryClicked(object? sender, EventArgs eventArgs)
    {
        if (_pageIndex == 0)
        {
            try
            {
                _options = _options with
                {
                    InstallRoot = InstallerOptions.NormalizeInstallRoot(_installRoot.Text),
                    OpenToolWizardAfterInstall = false,
                };
            }
            catch (ArgumentException ex)
            {
                MessageBox.Show(
                    this,
                    ex.Message,
                    "Choose another folder",
                    MessageBoxButtons.OK,
                    MessageBoxIcon.Warning);
                return;
            }

            await RunOperationAsync();
            return;
        }

        if (_pageIndex == 2)
        {
            CompleteAndClose();
        }
    }

    private async Task RunOperationAsync()
    {
        _operationRunning = true;
        _operationSucceeded = false;
        SetBusyState(true);
        ShowProgress(
            _options.Operation switch
            {
                InstallerOperation.Repair => "Repairing FOA-SDK",
                InstallerOperation.Uninstall => "Removing FOA-SDK",
                _ => "Installing FOA-SDK",
            },
            _options.Operation is InstallerOperation.Uninstall
                ? "Removing the application files from this PC…"
                : "Copying and registering the application files…");
        await Task.Yield();

        try
        {
            InstallerRunResult result = await WindowsInstallerRunner.RunAsync(_payload, _options);
            if (!result.Succeeded)
            {
                ShowFailure(result.Message);
                ExitCode = result.ExitCode == 0 ? 1 : result.ExitCode;
                return;
            }

            if (_options.Operation is not InstallerOperation.Uninstall)
            {
                ShowProgress(
                    "Validating installation",
                    "Checking installed files and startup requirements…");
                await Task.Yield();

                InstalledEditorValidationResult validation = await InstalledEditorLauncher.ValidateAsync(_options.InstallRoot);
                if (!validation.Succeeded)
                {
                    ShowFailure(validation.Message);
                    ExitCode = validation.ExitCode == 0 ? 1 : validation.ExitCode;
                    return;
                }
            }

            _operationSucceeded = true;
            ExitCode = 0;
            if (_options.Operation is InstallerOperation.Uninstall)
            {
                ShowSuccess("FOA-SDK was removed.", showFinishOptions: false);
            }
            else
            {
                ShowSuccess(
                    "Installation complete. The installed files passed validation and FOA-SDK is ready to use.",
                    showFinishOptions: true);
            }
        }
        catch (Exception ex) when (
            ex is IOException
                or UnauthorizedAccessException
                or InvalidOperationException
                or System.ComponentModel.Win32Exception)
        {
            ExitCode = 1;
            ShowFailure(ex.Message);
        }
        finally
        {
            _operationRunning = false;
            SetBusyState(false);
        }
    }

    private void ShowInstallPage()
    {
        _pageIndex = 0;
        ShowPage(_installPage);
        _primaryButton.Text = OperationButtonText();
        _primaryButton.Enabled = true;
        _primaryButton.Visible = true;
        _cancelButton.Enabled = true;
        _cancelButton.Visible = true;
    }

    private void ShowProgress(string title, string body)
    {
        _pageIndex = 1;
        _progressTitle.Text = title;
        _progressText.Text = body;
        ShowPage(_progressPage);
        _primaryButton.Visible = false;
        _cancelButton.Visible = true;
        _cancelButton.Enabled = false;
    }

    private void ShowSuccess(string message, bool showFinishOptions)
    {
        _pageIndex = 2;
        _resultTitle.Text = showFinishOptions ? "FOA-SDK is ready" : "Done";
        _resultTitle.ForeColor = Success;
        _resultText.Text = message;
        SetFinishOptionsVisible(showFinishOptions);
        ShowPage(_resultPage);
        _primaryButton.Text = "Finish";
        _primaryButton.Visible = true;
        _primaryButton.Enabled = true;
        _cancelButton.Visible = false;
    }

    private void ShowFailure(string message)
    {
        _pageIndex = 2;
        _operationSucceeded = false;
        _resultTitle.Text = "Setup could not be completed";
        _resultTitle.ForeColor = Error;
        _resultText.Text = message;
        SetFinishOptionsVisible(false);
        ShowPage(_resultPage);
        _primaryButton.Text = "Close";
        _primaryButton.Visible = true;
        _primaryButton.Enabled = true;
        _cancelButton.Visible = false;
    }

    private void SetFinishOptionsVisible(bool visible)
    {
        if (_resultPage?.Controls.Find("finish-options", false).FirstOrDefault() is Control optionsPanel)
        {
            optionsPanel.Visible = visible;
        }
    }

    private void ShowPage(Control? page)
    {
        if (page is null)
        {
            return;
        }
        _content.Controls.Clear();
        _content.Controls.Add(page);
    }

    private void SetBusyState(bool busy)
    {
        _installRoot.Enabled = !busy;
        _primaryButton.Enabled = !busy;
        _cancelButton.Enabled = !busy;
    }

    private void CompleteAndClose()
    {
        if (_operationSucceeded && _options.Operation is not InstallerOperation.Uninstall)
        {
            if (_createDesktopShortcut.Checked)
            {
                try
                {
                    InstalledEditorLauncher.CreateDesktopShortcut(_options.InstallRoot);
                }
                catch (Exception ex) when (
                    ex is IOException
                        or UnauthorizedAccessException
                        or InvalidOperationException
                        or System.ComponentModel.Win32Exception
                        or System.Runtime.InteropServices.COMException)
                {
                    MessageBox.Show(
                        this,
                        $"FOA-SDK is installed, but the desktop shortcut could not be created.\n\n{ex.Message}",
                        "Shortcut not created",
                        MessageBoxButtons.OK,
                        MessageBoxIcon.Warning);
                }
            }

            if (_launchEditor.Checked)
            {
                try
                {
                    InstalledEditorLauncher.Launch(_options.InstallRoot);
                }
                catch (Exception ex) when (ex is InvalidOperationException or System.ComponentModel.Win32Exception)
                {
                    MessageBox.Show(
                        this,
                        $"FOA-SDK is installed, but it could not be opened automatically.\n\n{ex.Message}",
                        "Unable to open FOA-SDK",
                        MessageBoxButtons.OK,
                        MessageBoxIcon.Warning);
                }
            }
        }
        Close();
    }
}
