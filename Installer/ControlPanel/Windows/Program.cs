// SPDX-License-Identifier: Apache-2.0 OR MIT
using System.Diagnostics;
using System.Drawing;
using System.Windows.Forms;

namespace FOA.SDK.ControlPanel;

internal sealed record ControlPanelOptions(
    string InstallRoot,
    bool SmokeTest,
    bool SelfTest,
    bool SaveProfile,
    bool NoDialog,
    string? WorkspaceRoot,
    string? GameInstallPath,
    string? ExportReportPath)
{
    public static ControlPanelOptions Parse(string[] args)
    {
        string installRoot = Path.TrimEndingDirectorySeparator(Path.GetFullPath(AppContext.BaseDirectory));
        bool smokeTest = false;
        bool selfTest = false;
        bool saveProfile = false;
        bool noDialog = false;
        string? workspace = null;
        string? gameInstall = null;
        string? exportReport = null;

        for (int index = 0; index < args.Length; ++index)
        {
            switch (args[index])
            {
                case "--install-root":
                    installRoot = RequireValue(args, ref index, "--install-root");
                    break;
                case "--smoke-test":
                    smokeTest = true;
                    noDialog = true;
                    break;
                case "--self-test":
                    selfTest = true;
                    noDialog = true;
                    break;
                case "--save-profile":
                    saveProfile = true;
                    noDialog = true;
                    break;
                case "--workspace-root":
                    workspace = RequireValue(args, ref index, "--workspace-root");
                    break;
                case "--game-install":
                    gameInstall = RequireValue(args, ref index, "--game-install");
                    break;
                case "--export-report":
                    exportReport = RequireValue(args, ref index, "--export-report");
                    noDialog = true;
                    break;
                case "--no-dialog":
                    noDialog = true;
                    break;
                default:
                    throw new ArgumentException($"Unknown Control Panel option: {args[index]}");
            }
        }
        return new ControlPanelOptions(
            installRoot,
            smokeTest,
            selfTest,
            saveProfile,
            noDialog,
            workspace,
            gameInstall,
            exportReport);
    }

    private static string RequireValue(string[] args, ref int index, string option)
    {
        if (index + 1 >= args.Length || args[index + 1].StartsWith("--", StringComparison.Ordinal))
        {
            throw new ArgumentException($"{option} requires a value.");
        }
        return args[++index];
    }
}

internal static class Program
{
    private const string Title = "FOA-SDK Control Panel";

    [STAThread]
    private static int Main(string[] args)
    {
        ApplicationConfiguration.Initialize();
        try
        {
            ControlPanelOptions options = ControlPanelOptions.Parse(args);
            if (options.SelfTest)
            {
                return SetupManagerCore.RunSelfTest();
            }

            if (options.SaveProfile || options.ExportReportPath is not null)
            {
                SetupManagerOptions loaded = SetupManagerCore.LoadOrDefault(options.InstallRoot);
                SetupManagerOptions requested = new(
                    options.WorkspaceRoot ?? loaded.WorkspaceRoot,
                    options.GameInstallPath ?? loaded.GameInstallPath);
                SetupManagerSaveResult saved = SetupManagerCore.Save(requested, options.InstallRoot);
                if (options.ExportReportPath is not null)
                {
                    _ = SetupManagerCore.ExportSupportReport(
                        requested,
                        options.InstallRoot,
                        options.ExportReportPath);
                }
                return saved.Readiness.ProductReady && saved.Readiness.WorkspaceReady ? 0 : 2;
            }

            using ControlPanelForm form = new(options.InstallRoot);
            if (options.SmokeTest)
            {
                form.ValidateSmokeContract();
                return 0;
            }
            Application.Run(form);
            return form.ExitCode;
        }
        catch (Exception ex) when (
            ex is ArgumentException
                or IOException
                or InvalidOperationException
                or UnauthorizedAccessException
                or System.Text.Json.JsonException)
        {
            if (args.Contains("--no-dialog", StringComparer.Ordinal)
                || args.Contains("--smoke-test", StringComparer.Ordinal)
                || args.Contains("--self-test", StringComparer.Ordinal)
                || args.Contains("--save-profile", StringComparer.Ordinal)
                || args.Contains("--export-report", StringComparer.Ordinal))
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
}

internal sealed class ControlPanelForm : Form
{
    private static readonly Color Header = Color.FromArgb(31, 43, 40);
    private static readonly Color Accent = Color.FromArgb(211, 164, 67);
    private static readonly Color Success = Color.FromArgb(34, 112, 74);
    private static readonly Color Warning = Color.FromArgb(156, 92, 22);
    private static readonly Font BaseFont = new(
        (SystemFonts.MessageBoxFont ?? SystemFonts.DefaultFont).FontFamily,
        9f,
        FontStyle.Regular);

    private readonly string _installRoot;
    private readonly ProviderManifest _provider;
    private SetupManagerOptions _options;
    private SetupManagerReadiness? _readiness;
    private readonly TabControl _tabs = new() { Dock = DockStyle.Fill, Padding = new Point(18, 7) };
    private readonly TextBox _workspaceRoot = NewPathBox("External workspace folder");
    private readonly TextBox _gameInstall = NewPathBox("Fall of Avalon installation folder");
    private readonly Label _headline = NewStatusLabel(18f);
    private readonly Label _productStatus = NewStatusLabel(10f);
    private readonly Label _workspaceStatus = NewStatusLabel(10f);
    private readonly Label _gameStatus = NewStatusLabel(10f);
    private readonly Label _routeStatus = NewStatusLabel(10f);
    private readonly RichTextBox _compatibilityReport = NewReportBox();
    private readonly RichTextBox _diagnosticsReport = NewReportBox();
    private readonly Label _savedProfile = new() { AutoSize = false, Height = 38, Font = BaseFont };

    public ControlPanelForm(string installRoot)
    {
        _installRoot = Path.TrimEndingDirectorySeparator(Path.GetFullPath(installRoot));
        _provider = ProviderManifestLoader.LoadBundledFoaProvider();
        try
        {
            _options = SetupManagerCore.LoadOrDefault(_installRoot);
        }
        catch (Exception ex) when (
            ex is IOException
                or InvalidOperationException
                or ArgumentException
                or UnauthorizedAccessException
                or System.Text.Json.JsonException)
        {
            _options = new SetupManagerOptions(SetupManagerCore.DefaultWorkspaceRoot(), null);
            _savedProfile.Text = $"Saved profile needs attention: {ex.Message}";
        }

        Text = "FOA-SDK Control Panel";
        StartPosition = FormStartPosition.CenterScreen;
        MinimumSize = new Size(920, 680);
        Size = new Size(1040, 760);
        AutoScaleMode = AutoScaleMode.Dpi;
        Font = BaseFont;
        BackColor = SystemColors.Window;
        KeyPreview = true;
        ExitCode = 0;

        _workspaceRoot.Text = _options.WorkspaceRoot;
        _gameInstall.Text = _options.GameInstallPath ?? string.Empty;
        _tabs.TabPages.Add(BuildHomePage());
        _tabs.TabPages.Add(BuildSetupPage());
        _tabs.TabPages.Add(BuildCompatibilityPage());
        _tabs.TabPages.Add(BuildDiagnosticsPage());

        TableLayoutPanel root = new()
        {
            Dock = DockStyle.Fill,
            ColumnCount = 1,
            RowCount = 3,
        };
        root.RowStyles.Add(new RowStyle(SizeType.Absolute, 92));
        root.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
        root.RowStyles.Add(new RowStyle(SizeType.Absolute, 78));
        root.Controls.Add(BuildHeader(), 0, 0);
        root.Controls.Add(_tabs, 0, 1);
        root.Controls.Add(BuildFooter(), 0, 2);
        Controls.Add(root);

        Shown += (_, _) => RefreshReadiness(showError: false);
        RefreshReadiness(showError: false);
    }

    public int ExitCode { get; private set; }

    public void ValidateSmokeContract()
    {
        CreateControl();
        string[] expectedTabs = { "Home", "Setup", "Compatibility", "Diagnostics" };
        if (_tabs.TabPages.Count != expectedTabs.Length
            || !_tabs.TabPages.Cast<TabPage>()
                .Select(page => page.Text)
                .SequenceEqual(expectedTabs, StringComparer.Ordinal))
        {
            throw new InvalidOperationException("The Control Panel page contract is incomplete.");
        }
        if (AcceptButton is null || CancelButton is null
            || MinimumSize.Width < 900 || MinimumSize.Height < 650
            || string.IsNullOrWhiteSpace(_workspaceRoot.AccessibleName)
            || string.IsNullOrWhiteSpace(_gameInstall.AccessibleName))
        {
            throw new InvalidOperationException("The Control Panel accessibility or window contract is incomplete.");
        }

        HashSet<string> buttonLabels = Descendants(this)
            .OfType<Button>()
            .Select(button => button.Text)
            .ToHashSet(StringComparer.Ordinal);
        foreach (string label in new[] { "Review setup", "Save changes", "Check again", "Open FOA-SDK", "Close" })
        {
            if (!buttonLabels.Contains(label))
            {
                throw new InvalidOperationException($"The Control Panel is missing the '{label}' action.");
            }
        }
    }

    protected override bool ProcessCmdKey(ref Message message, Keys keyData)
    {
        if (keyData == (Keys.Control | Keys.S))
        {
            SaveProfile();
            return true;
        }
        if (keyData == Keys.F5)
        {
            RefreshReadiness(showError: true);
            return true;
        }
        return base.ProcessCmdKey(ref message, keyData);
    }

    private Control BuildHeader()
    {
        Panel header = new() { Dock = DockStyle.Fill, BackColor = Header, Padding = new Padding(32, 18, 32, 12) };
        Panel accent = new() { BackColor = Accent, Size = new Size(6, 54), Location = new Point(18, 18) };
        Label title = new()
        {
            Text = "FOA-SDK Control Panel",
            AutoSize = true,
            Font = new Font(BaseFont.FontFamily, 15f, FontStyle.Bold),
            ForeColor = Color.White,
            Location = new Point(36, 18),
        };
        Label subtitle = new()
        {
            Text = "Installed setup manager · local, read-only game readiness",
            AutoSize = true,
            Font = BaseFont,
            ForeColor = Color.FromArgb(190, 205, 199),
            Location = new Point(38, 53),
        };
        header.Controls.Add(accent);
        header.Controls.Add(title);
        header.Controls.Add(subtitle);
        return header;
    }

    private TabPage BuildHomePage()
    {
        TabPage page = NewTab("Home", "FOA-SDK setup overview");
        FlowLayoutPanel content = NewVerticalContent();
        Label title = NewHeading("Your authoring setup");
        Label intro = NewBody(
            "This panel checks the installed SDK, your external workspace, and one local Fall of Avalon folder. "
            + "It never scans the whole PC, modifies the game, installs a loader, or launches the game.");
        _headline.Height = 48;
        _headline.AccessibleName = "Overall readiness";

        TableLayoutPanel cards = new()
        {
            Width = 860,
            Height = 178,
            ColumnCount = 2,
            RowCount = 2,
            Margin = new Padding(0, 12, 0, 8),
        };
        cards.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 50));
        cards.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 50));
        cards.RowStyles.Add(new RowStyle(SizeType.Percent, 50));
        cards.RowStyles.Add(new RowStyle(SizeType.Percent, 50));
        cards.Controls.Add(StatusCard("FOA-SDK", _productStatus), 0, 0);
        cards.Controls.Add(StatusCard("Workspace", _workspaceStatus), 1, 0);
        cards.Controls.Add(StatusCard("Fall of Avalon", _gameStatus), 0, 1);
        cards.Controls.Add(StatusCard("Runtime route", _routeStatus), 1, 1);

        Button setupButton = NewButton("Review setup", "Open workspace and game-path settings.");
        setupButton.Click += (_, _) => _tabs.SelectedIndex = 1;
        Button reportButton = NewButton("View compatibility", "Open the read-only compatibility report.");
        reportButton.Click += (_, _) => _tabs.SelectedIndex = 2;
        FlowLayoutPanel actions = new() { Width = 860, Height = 50, FlowDirection = FlowDirection.LeftToRight };
        actions.Controls.Add(setupButton);
        actions.Controls.Add(reportButton);

        content.Controls.Add(title);
        content.Controls.Add(intro);
        content.Controls.Add(_headline);
        content.Controls.Add(cards);
        content.Controls.Add(actions);
        page.Controls.Add(content);
        return page;
    }

    private TabPage BuildSetupPage()
    {
        TabPage page = NewTab("Setup", "Workspace and local game binding");
        FlowLayoutPanel content = NewVerticalContent();
        content.Controls.Add(NewHeading("Workspace and game"));
        content.Controls.Add(NewBody(
            "Choose an external workspace for authored files. The game folder is optional until you want a local compatibility report."));
        content.Controls.Add(PathCard(
            "External workspace",
            "FOA-SDK can create this folder when you save. It must remain outside the installed application.",
            _workspaceRoot,
            () => BrowseFolder(_workspaceRoot, "Choose the external FOA-SDK workspace", true)));
        content.Controls.Add(PathCard(
            "Fall of Avalon",
            "Choose the game folder itself. Validation reads only a bounded set of top-level Unity/runtime markers.",
            _gameInstall,
            () => BrowseFolder(_gameInstall, "Choose the Fall of Avalon installation", false)));

        Label boundary = NewBody(
            "Safety boundary: setup does not write to the selected game path and does not enable conversion, deployment, game launch, or save access.");
        boundary.ForeColor = Warning;
        content.Controls.Add(boundary);
        content.Controls.Add(_savedProfile);
        page.Controls.Add(content);
        return page;
    }

    private TabPage BuildCompatibilityPage()
    {
        TabPage page = NewTab("Compatibility", "Read-only provider and runtime report");
        TableLayoutPanel content = NewReportLayout(
            "Compatibility report",
            "Observed markers indicate a candidate runtime route only. They are not live-runtime proof or deployment authority.");
        _compatibilityReport.AccessibleName = "Compatibility report";
        content.Controls.Add(_compatibilityReport, 0, 2);
        page.Controls.Add(content);
        return page;
    }

    private TabPage BuildDiagnosticsPage()
    {
        TabPage page = NewTab("Diagnostics", "Redacted support and non-mutating plan");
        TableLayoutPanel content = NewReportLayout(
            "Diagnostics and plan preview",
            "Export creates one redacted JSON report. It contains no game files, save data, credentials, or full private paths.");
        _diagnosticsReport.AccessibleName = "Diagnostics and plan preview";
        content.Controls.Add(_diagnosticsReport, 0, 2);
        Button exportButton = NewButton("Export redacted report...", "Save a redacted JSON support report.");
        exportButton.Click += (_, _) => ExportReport();
        FlowLayoutPanel row = new() { Dock = DockStyle.Fill, FlowDirection = FlowDirection.LeftToRight };
        row.Controls.Add(exportButton);
        content.Controls.Add(row, 0, 3);
        page.Controls.Add(content);
        return page;
    }

    private Control BuildFooter()
    {
        FlowLayoutPanel footer = new()
        {
            Dock = DockStyle.Fill,
            FlowDirection = FlowDirection.RightToLeft,
            WrapContents = false,
            Padding = new Padding(24, 18, 28, 12),
            BackColor = SystemColors.ControlLightLight,
        };
        Button close = NewButton("Close", "Close the Control Panel.");
        close.Click += (_, _) => Close();
        Button save = NewButton("Save changes", "Validate and save the local setup profile. Shortcut: Ctrl+S.");
        save.Click += (_, _) => SaveProfile();
        Button recheck = NewButton("Check again", "Re-run bounded local checks. Shortcut: F5.");
        recheck.Click += (_, _) => RefreshReadiness(showError: true);
        Button openEditor = NewButton("Open FOA-SDK", "Open the installed authoring Editor.");
        openEditor.Click += (_, _) => OpenEditor();
        footer.Controls.Add(close);
        footer.Controls.Add(save);
        footer.Controls.Add(recheck);
        footer.Controls.Add(openEditor);
        AcceptButton = save;
        CancelButton = close;
        return footer;
    }

    private void RefreshReadiness(bool showError)
    {
        try
        {
            _options = SetupManagerCore.Normalize(
                new SetupManagerOptions(_workspaceRoot.Text, EmptyToNull(_gameInstall.Text)),
                _installRoot);
            _readiness = SetupManagerCore.Evaluate(_options, _installRoot);
            bool ready = _readiness.ProductReady && _readiness.WorkspaceReady && _readiness.Game.IsValid;
            _headline.Text = ready ? "Read-only setup is ready" : "Setup needs attention";
            _headline.ForeColor = ready ? Success : Warning;
            _productStatus.Text = _readiness.ProductReady ? "Ready" : "Repair required";
            _workspaceStatus.Text = _readiness.WorkspaceReady ? "External and ready" : "Blocked";
            _gameStatus.Text = _readiness.Game.State;
            _routeStatus.Text = _readiness.Game.RuntimeRoute;
            string description = SetupManagerCore.Describe(_readiness);
            _compatibilityReport.Text = $"Provider: {_provider.DisplayName}{Environment.NewLine}"
                + $"Provider ID: {_provider.ProviderId}{Environment.NewLine}{Environment.NewLine}"
                + description;
            _diagnosticsReport.Text = description;
        }
        catch (Exception ex) when (ex is ArgumentException or IOException or InvalidOperationException or UnauthorizedAccessException)
        {
            _readiness = null;
            _headline.Text = "Setup needs attention";
            _headline.ForeColor = Warning;
            _productStatus.Text = "Unavailable";
            _workspaceStatus.Text = "Blocked";
            _gameStatus.Text = "Unavailable";
            _routeStatus.Text = "unknown";
            _compatibilityReport.Text = ex.Message;
            _diagnosticsReport.Text = ex.Message;
            if (showError)
            {
                MessageBox.Show(this, ex.Message, "Setup check could not complete", MessageBoxButtons.OK, MessageBoxIcon.Warning);
            }
        }
    }

    private void SaveProfile()
    {
        try
        {
            _options = new SetupManagerOptions(_workspaceRoot.Text, EmptyToNull(_gameInstall.Text));
            SetupManagerSaveResult result = SetupManagerCore.Save(_options, _installRoot);
            _options = SetupManagerCore.Normalize(_options, _installRoot);
            _workspaceRoot.Text = _options.WorkspaceRoot;
            _gameInstall.Text = _options.GameInstallPath ?? string.Empty;
            _savedProfile.Text = $"Saved locally: {result.ProfilePath}";
            ExitCode = 0;
            RefreshReadiness(showError: false);
        }
        catch (Exception ex) when (ex is ArgumentException or IOException or InvalidOperationException or UnauthorizedAccessException)
        {
            ExitCode = 1;
            MessageBox.Show(this, ex.Message, "Profile not saved", MessageBoxButtons.OK, MessageBoxIcon.Error);
        }
    }

    private void ExportReport()
    {
        RefreshReadiness(showError: true);
        if (_readiness is null)
        {
            return;
        }
        using SaveFileDialog dialog = new()
        {
            Title = "Export redacted FOA-SDK support report",
            Filter = "JSON report|*.json",
            FileName = $"foa-sdk-support-{DateTime.UtcNow:yyyyMMdd-HHmmss}.json",
            AddExtension = true,
            DefaultExt = "json",
        };
        if (dialog.ShowDialog(this) != DialogResult.OK)
        {
            return;
        }
        try
        {
            string path = SetupManagerCore.ExportSupportReport(_options, _installRoot, dialog.FileName);
            MessageBox.Show(this, $"Redacted support report saved.\n\n{path}", "Report exported", MessageBoxButtons.OK, MessageBoxIcon.Information);
        }
        catch (Exception ex) when (ex is ArgumentException or IOException or InvalidOperationException or UnauthorizedAccessException)
        {
            MessageBox.Show(this, ex.Message, "Report not exported", MessageBoxButtons.OK, MessageBoxIcon.Error);
        }
    }

    private void OpenEditor()
    {
        string launcher = Path.Combine(_installRoot, "bin", "Windows", "profile", "Default", "FOA-SDK.exe");
        if (!File.Exists(launcher))
        {
            MessageBox.Show(this, "FOA-SDK.exe is missing. Run installer Repair.", "Unable to open FOA-SDK", MessageBoxButtons.OK, MessageBoxIcon.Error);
            return;
        }
        try
        {
            _ = Process.Start(new ProcessStartInfo
            {
                FileName = launcher,
                WorkingDirectory = Path.GetDirectoryName(launcher)!,
                UseShellExecute = true,
            });
        }
        catch (Exception ex) when (
            ex is System.ComponentModel.Win32Exception or InvalidOperationException or IOException)
        {
            MessageBox.Show(this, ex.Message, "Unable to open FOA-SDK", MessageBoxButtons.OK, MessageBoxIcon.Error);
        }
    }

    private void BrowseFolder(TextBox target, string description, bool createAllowed)
    {
        string initial = Directory.Exists(target.Text)
            ? target.Text
            : Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
        using FolderBrowserDialog dialog = new()
        {
            Description = description,
            InitialDirectory = initial,
            SelectedPath = initial,
            ShowNewFolderButton = createAllowed,
            UseDescriptionForTitle = true,
        };
        if (dialog.ShowDialog(this) == DialogResult.OK)
        {
            target.Text = dialog.SelectedPath;
            RefreshReadiness(showError: false);
        }
    }

    private static TabPage NewTab(string text, string accessibleDescription) => new(text)
    {
        BackColor = SystemColors.Window,
        Padding = new Padding(26),
        UseVisualStyleBackColor = false,
        AccessibleName = text,
        AccessibleDescription = accessibleDescription,
    };

    private static FlowLayoutPanel NewVerticalContent() => new()
    {
        Dock = DockStyle.Fill,
        FlowDirection = FlowDirection.TopDown,
        WrapContents = false,
        AutoScroll = true,
        Padding = new Padding(14),
    };

    private static TableLayoutPanel NewReportLayout(string heading, string body)
    {
        TableLayoutPanel layout = new() { Dock = DockStyle.Fill, ColumnCount = 1, RowCount = 4, Padding = new Padding(14) };
        layout.RowStyles.Add(new RowStyle(SizeType.Absolute, 45));
        layout.RowStyles.Add(new RowStyle(SizeType.Absolute, 54));
        layout.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
        layout.RowStyles.Add(new RowStyle(SizeType.Absolute, 54));
        layout.Controls.Add(NewHeading(heading), 0, 0);
        layout.Controls.Add(NewBody(body), 0, 1);
        return layout;
    }

    private static Panel StatusCard(string heading, Label value)
    {
        Panel panel = new() { Dock = DockStyle.Fill, Margin = new Padding(6), Padding = new Padding(16), BorderStyle = BorderStyle.FixedSingle };
        Label label = new() { Text = heading, AutoSize = true, Font = new Font(BaseFont, FontStyle.Bold), Location = new Point(14, 12) };
        value.Location = new Point(14, 40);
        value.Size = new Size(370, 30);
        value.AccessibleName = $"{heading} status";
        panel.Controls.Add(label);
        panel.Controls.Add(value);
        return panel;
    }

    private static Panel PathCard(string heading, string body, TextBox box, Action browse)
    {
        Panel panel = new() { Width = 860, Height = 142, Margin = new Padding(0, 10, 0, 4), Padding = new Padding(16), BorderStyle = BorderStyle.FixedSingle };
        Label title = new() { Text = heading, AutoSize = true, Font = new Font(BaseFont, FontStyle.Bold), Location = new Point(16, 12) };
        Label description = NewBody(body);
        description.Location = new Point(16, 36);
        description.Size = new Size(810, 38);
        box.Location = new Point(16, 88);
        box.Size = new Size(700, 28);
        Button button = NewButton("Browse...", $"Browse for {heading}.");
        button.Location = new Point(730, 85);
        button.Click += (_, _) => browse();
        panel.Controls.Add(title);
        panel.Controls.Add(description);
        panel.Controls.Add(box);
        panel.Controls.Add(button);
        return panel;
    }

    private static TextBox NewPathBox(string accessibleName) => new()
    {
        Font = BaseFont,
        BorderStyle = BorderStyle.FixedSingle,
        AccessibleName = accessibleName,
    };

    private static Label NewHeading(string text) => new()
    {
        Text = text,
        AutoSize = false,
        Width = 860,
        Height = 42,
        Font = new Font(BaseFont.FontFamily, 16f, FontStyle.Bold),
    };

    private static Label NewBody(string text) => new()
    {
        Text = text,
        AutoSize = false,
        Width = 860,
        Height = 58,
        Font = BaseFont,
    };

    private static Label NewStatusLabel(float size) => new()
    {
        AutoSize = false,
        Font = new Font(BaseFont.FontFamily, size, FontStyle.Bold),
    };

    private static RichTextBox NewReportBox() => new()
    {
        Dock = DockStyle.Fill,
        ReadOnly = true,
        DetectUrls = false,
        BackColor = SystemColors.Window,
        Font = new Font(FontFamily.GenericMonospace, 9f),
        BorderStyle = BorderStyle.FixedSingle,
    };

    private static Button NewButton(string text, string accessibleDescription) => new()
    {
        Text = text,
        Width = 142,
        Height = 38,
        FlatStyle = FlatStyle.System,
        AccessibleName = text,
        AccessibleDescription = accessibleDescription,
        Margin = new Padding(8, 0, 0, 0),
    };

    private static string? EmptyToNull(string value) => string.IsNullOrWhiteSpace(value) ? null : value;

    private static IEnumerable<Control> Descendants(Control parent)
    {
        foreach (Control child in parent.Controls)
        {
            yield return child;
            foreach (Control descendant in Descendants(child))
            {
                yield return descendant;
            }
        }
    }
}
