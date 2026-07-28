// SPDX-License-Identifier: Apache-2.0 OR MIT
using System.Diagnostics;
using System.Drawing;
using System.Windows.Forms;

namespace FOA.SDK.InstallerLauncher;

internal sealed class ToolSetupWizardForm : Form
{
    private readonly string _installRoot;
    private ToolSetupOptions _options;
    private ToolSetupReadiness? _readiness;
    private readonly Panel _content = new() { Dock = DockStyle.Fill, Padding = new Padding(28) };
    private readonly Button _backButton = new() { Text = "< Back", Width = 92, Height = 32 };
    private readonly Button _nextButton = new() { Text = "Next >", Width = 92, Height = 32 };
    private readonly Button _cancelButton = new() { Text = "Cancel", Width = 92, Height = 32 };
    private readonly TextBox _workspaceRoot = new() { Width = 470 };
    private readonly TextBox _o3deEditorPath = new() { Width = 470 };
    private readonly TextBox _unityEditorPath = new() { Width = 470 };
    private readonly TextBox _unityProjectPath = new() { Width = 470 };
    private readonly TextBox _taintedGrailInstallPath = new() { Width = 470 };
    private readonly Label _reviewText = NewBodyLabel();
    private readonly Label _resultText = NewBodyLabel();
    private readonly List<Control> _pages = new();
    private int _pageIndex;

    public ToolSetupWizardForm(string installRoot)
    {
        _installRoot = InstallerOptions.NormalizeInstallRoot(installRoot);
        _options = ToolSetupProfile.LoadOrDefault(_installRoot);
        ExitCode = 1;
        Text = "FOA-SDK Tool Wizard";
        StartPosition = FormStartPosition.CenterScreen;
        MinimumSize = new Size(820, 620);
        Size = new Size(860, 660);
        FormBorderStyle = FormBorderStyle.FixedDialog;
        MaximizeBox = false;
        MinimizeBox = true;
        AutoScaleMode = AutoScaleMode.Dpi;

        Controls.Add(_content);
        Controls.Add(BuildButtonBar());
        LoadOptionsIntoFields();
        _pages.Add(BuildWelcomePage());
        _pages.Add(BuildToolPathsPage());
        _pages.Add(BuildReviewPage());
        _pages.Add(BuildResultPage());

        _backButton.Click += (_, _) => SetPage(_pageIndex - 1);
        _nextButton.Click += NextClicked;
        _cancelButton.Click += (_, _) => Close();
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
            "Welcome to the Tool Wizard",
            "This wizard configures local FOA-SDK authoring tools after the product is installed.\n\n"
            + "It records an external workspace plus optional O3DE, Unity conversion, and Tainted Grail paths.\n\n"
            + "It does not run Windows Installer, mutate game files, deploy adapters, or execute Unity conversion.");
    }

    private Control BuildToolPathsPage()
    {
        Panel page = BuildPage(
            "Choose local tools",
            "The workspace is required and must live outside the installed product. Tool paths can be left blank and completed later.");
        TableLayoutPanel grid = new()
        {
            ColumnCount = 3,
            RowCount = 5,
            Location = new Point(32, 145),
            Size = new Size(745, 240),
            AutoSize = true,
        };
        grid.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 155));
        grid.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 500));
        grid.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 90));
        AddToolRow(grid, 0, "Workspace:", _workspaceRoot, () => BrowseFolder(_workspaceRoot, "Choose the external FOA workspace directory", true));
        AddToolRow(grid, 1, "O3DE Editor:", _o3deEditorPath, () => BrowseExecutable(_o3deEditorPath, "Select O3DE Editor.exe", "O3DE Editor|Editor.exe|Executables|*.exe"));
        AddToolRow(grid, 2, "Unity Editor:", _unityEditorPath, () => BrowseExecutable(_unityEditorPath, "Select Unity.exe", "Unity Editor|Unity.exe|Executables|*.exe"));
        AddToolRow(grid, 3, "Unity project:", _unityProjectPath, () => BrowseFolder(_unityProjectPath, "Choose the Unity conversion project directory", false));
        AddToolRow(grid, 4, "TG install:", _taintedGrailInstallPath, () => BrowseFolder(_taintedGrailInstallPath, "Choose the local Tainted Grail install directory", false));
        page.Controls.Add(grid);
        return page;
    }

    private Control BuildReviewPage()
    {
        Panel page = BuildPage("Review tool readiness", string.Empty);
        _reviewText.Location = new Point(32, 105);
        _reviewText.Size = new Size(740, 430);
        page.Controls.Add(_reviewText);
        return page;
    }

    private Control BuildResultPage()
    {
        Panel page = BuildPage("Tool Wizard result", string.Empty);
        _resultText.Location = new Point(32, 115);
        _resultText.Size = new Size(740, 320);
        page.Controls.Add(_resultText);
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
        text.Size = new Size(700, 300);
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

    private static void AddToolRow(
        TableLayoutPanel grid,
        int row,
        string labelText,
        TextBox target,
        Action browse)
    {
        Label label = new()
        {
            Text = labelText,
            AutoSize = true,
            Anchor = AnchorStyles.Left,
            Margin = new Padding(0, 6, 8, 6),
        };
        Button button = new() { Text = "Browse...", Width = 82 };
        button.Click += (_, _) => browse();
        target.Anchor = AnchorStyles.Left | AnchorStyles.Right;
        grid.Controls.Add(label, 0, row);
        grid.Controls.Add(target, 1, row);
        grid.Controls.Add(button, 2, row);
    }

    private void LoadOptionsIntoFields()
    {
        _workspaceRoot.Text = _options.WorkspaceRoot;
        _o3deEditorPath.Text = _options.O3deEditorPath ?? string.Empty;
        _unityEditorPath.Text = _options.UnityEditorPath ?? string.Empty;
        _unityProjectPath.Text = _options.UnityProjectPath ?? string.Empty;
        _taintedGrailInstallPath.Text = _options.TaintedGrailInstallPath ?? string.Empty;
    }

    private void BrowseFolder(TextBox target, string description, bool showNewFolderButton)
    {
        using FolderBrowserDialog dialog = new()
        {
            Description = description,
            InitialDirectory = InitialDirectoryFor(target.Text),
            ShowNewFolderButton = showNewFolderButton,
        };
        if (dialog.ShowDialog(this) == DialogResult.OK)
        {
            target.Text = dialog.SelectedPath;
        }
    }

    private void BrowseExecutable(TextBox target, string title, string filter)
    {
        using OpenFileDialog dialog = new()
        {
            Title = title,
            Filter = filter,
            CheckFileExists = true,
            InitialDirectory = InitialDirectoryFor(target.Text),
        };
        if (dialog.ShowDialog(this) == DialogResult.OK)
        {
            target.Text = dialog.FileName;
        }
    }

    private static string InitialDirectoryFor(string current)
    {
        if (Directory.Exists(current))
        {
            return current;
        }
        string? parent = string.IsNullOrWhiteSpace(current) ? null : Path.GetDirectoryName(current);
        if (!string.IsNullOrWhiteSpace(parent) && Directory.Exists(parent))
        {
            return parent;
        }
        string localAppData = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
        return string.IsNullOrWhiteSpace(localAppData)
            ? Environment.GetFolderPath(Environment.SpecialFolder.UserProfile)
            : localAppData;
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
            catch (Exception ex) when (ex is ArgumentException or InvalidOperationException)
            {
                MessageBox.Show(this, ex.Message, "Invalid tool path", MessageBoxButtons.OK, MessageBoxIcon.Warning);
            }
            return;
        }
        if (_pageIndex == 2)
        {
            try
            {
                ToolSetupSaveResult result = ToolSetupProfile.Save(_options, _installRoot);
                ExitCode = 0;
                _resultText.Text = "Tool profile saved.\n\n"
                    + $"Profile: {result.ProfilePath}\n\n"
                    + $"Authoring ready: {YesNo(result.Readiness.ReadyForAuthoring)}\n"
                    + $"Conversion preview ready: {YesNo(result.Readiness.ReadyForConversionPreview)}\n"
                    + $"Deployment review ready: {YesNo(result.Readiness.ReadyForDeploymentPreview)}";
                SetPage(3);
            }
            catch (Exception ex) when (ex is IOException or UnauthorizedAccessException or ArgumentException or InvalidOperationException)
            {
                MessageBox.Show(this, ex.Message, "Tool Wizard save failed", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
            return;
        }
        if (_pageIndex == 3)
        {
            Close();
        }
    }

    private void CaptureOptions()
    {
        _options = ToolSetupProfile.Normalize(
            new ToolSetupOptions(
                _workspaceRoot.Text,
                EmptyToNull(_o3deEditorPath.Text),
                EmptyToNull(_unityEditorPath.Text),
                EmptyToNull(_unityProjectPath.Text),
                EmptyToNull(_taintedGrailInstallPath.Text)),
            _installRoot);
        _readiness = ToolSetupProfile.Validate(_options, _installRoot);
    }

    private void UpdateReview()
    {
        ToolSetupReadiness readiness = _readiness
            ?? ToolSetupProfile.Validate(_options, _installRoot);
        _reviewText.Text = $"Installed FOA-SDK root: {_installRoot}{Environment.NewLine}{Environment.NewLine}"
            + ToolSetupProfile.Describe(_options, readiness);
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
        _backButton.Enabled = index is 1 or 2;
        _cancelButton.Enabled = index != 3;
        _nextButton.Text = index switch
        {
            2 => "Save",
            3 => "Finish",
            _ => "Next >",
        };
    }

    private static string? EmptyToNull(string value) => string.IsNullOrWhiteSpace(value) ? null : value;

    private static string YesNo(bool value) => value ? "yes" : "no";
}

internal static class ToolSetupWizardLauncher
{
    public static void Launch(string installRoot)
    {
        string? executable = Environment.ProcessPath;
        if (string.IsNullOrWhiteSpace(executable))
        {
            executable = Application.ExecutablePath;
        }
        if (string.IsNullOrWhiteSpace(executable) || !File.Exists(executable))
        {
            throw new InvalidOperationException("FOA-SDK-Installer.exe could not locate itself to open the Tool Wizard.");
        }

        ProcessStartInfo startInfo = new()
        {
            FileName = executable,
            UseShellExecute = false,
        };
        startInfo.ArgumentList.Add("--tool-wizard");
        startInfo.ArgumentList.Add("--install-root");
        startInfo.ArgumentList.Add(installRoot);
        _ = Process.Start(startInfo)
            ?? throw new InvalidOperationException("FOA-SDK Tool Wizard did not start.");
    }
}
