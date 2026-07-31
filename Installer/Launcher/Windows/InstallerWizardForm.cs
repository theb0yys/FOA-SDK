// SPDX-License-Identifier: Apache-2.0 OR MIT
using System.Diagnostics;
using System.Drawing;
using System.Windows.Forms;

namespace FOA.SDK.InstallerLauncher;

internal sealed class InstallerWizardForm : Form
{
    private const int RailWidth = 320;
    private const int PageContentMinWidth = 560;
    private const int PageContentMaxWidth = 1040;
    private const int CardPadding = 18;
    private const int CardGap = 14;
    private const string PageContentTag = "foa-page-content";

    private static readonly Color ShellBackground = Color.FromArgb(246, 248, 247);
    private static readonly Color Surface = Color.White;
    private static readonly Color RailBackground = Color.FromArgb(33, 47, 43);
    private static readonly Color RailMuted = Color.FromArgb(179, 196, 188);
    private static readonly Color Accent = Color.FromArgb(216, 168, 70);
    private static readonly Color AccentSoft = Color.FromArgb(254, 246, 225);
    private static readonly Color Border = Color.FromArgb(206, 216, 211);
    private static readonly Color Ink = Color.FromArgb(31, 39, 37);
    private static readonly Color Muted = Color.FromArgb(92, 106, 101);
    private static readonly Font BaseFont = new(
        (SystemFonts.MessageBoxFont ?? SystemFonts.DefaultFont).FontFamily,
        8.75f,
        FontStyle.Regular);

    private readonly InstallerPayload _payload;
    private InstallerOptions _options;
    private readonly Panel _content = new() { Dock = DockStyle.Fill, Padding = new Padding(32, 28, 32, 24), BackColor = ShellBackground };
    private readonly Button _backButton = NewNavButton("< Back");
    private readonly Button _nextButton = NewPrimaryButton("Next >");
    private readonly Button _cancelButton = NewNavButton("Cancel");
    private readonly RadioButton _installRadio = new() { Text = "Install or upgrade the complete FOA-SDK", AutoSize = true, Checked = true };
    private readonly RadioButton _repairRadio = new() { Text = "Repair the installed FOA-SDK", AutoSize = true };
    private readonly RadioButton _uninstallRadio = new() { Text = "Uninstall FOA-SDK", AutoSize = true };
    private readonly TextBox _installRoot = new() { Width = 560 };
    private readonly Label _reviewText = NewBodyLabel();
    private readonly Label _resultText = NewBodyLabel();
    private readonly Label _resultStatus = NewStatusLabel();
    private readonly Label _resultLogLabel = NewSectionTitle("MSI log path");
    private readonly TextBox _resultLogPath = NewResultPathBox();
    private readonly Button _openLogFolderButton = NewSmallButton("Open log folder");
    private readonly CheckBox _launchEditor = new() { Text = "Launch installed SDK editor (FOA-SDK.exe)", AutoSize = true, Checked = true };
    private readonly CheckBox _openToolWizard = new() { Text = "Open separate Tool Setup Wizard", AutoSize = true, Checked = true };
    private readonly ProgressBar _progress = new() { Style = ProgressBarStyle.Marquee, MarqueeAnimationSpeed = 30, Height = 18 };
    private readonly List<Control> _pages = new();
    private readonly Label[] _stepLabels = new Label[5];
    private readonly Panel[] _operationCards = new Panel[3];
    private int _pageIndex;
    private bool _operationRunning;
    private bool _operationSucceeded;

    private sealed class WrappingLabel : Label
    {
        public WrappingLabel()
        {
            SetStyle(
                ControlStyles.UserPaint |
                ControlStyles.AllPaintingInWmPaint |
                ControlStyles.OptimizedDoubleBuffer,
                true);
        }

        protected override void OnPaint(PaintEventArgs eventArgs)
        {
            using StringFormat format = new()
            {
                Trimming = StringTrimming.None,
            };
            format.Alignment = TextAlign switch
            {
                ContentAlignment.TopCenter or ContentAlignment.MiddleCenter or ContentAlignment.BottomCenter => StringAlignment.Center,
                ContentAlignment.TopRight or ContentAlignment.MiddleRight or ContentAlignment.BottomRight => StringAlignment.Far,
                _ => StringAlignment.Near,
            };
            format.LineAlignment = TextAlign switch
            {
                ContentAlignment.MiddleLeft or ContentAlignment.MiddleCenter or ContentAlignment.MiddleRight => StringAlignment.Center,
                ContentAlignment.BottomLeft or ContentAlignment.BottomCenter or ContentAlignment.BottomRight => StringAlignment.Far,
                _ => StringAlignment.Near,
            };
            using SolidBrush brush = new(ForeColor);
            eventArgs.Graphics.DrawString(Text, Font, brush, ClientRectangle, format);
        }
    }

    public InstallerWizardForm(InstallerPayload payload, InstallerOptions options)
    {
        _payload = payload;
        _options = options;
        ExitCode = 1;
        Text = "FOA-SDK Guided Installer (EXE)";
        StartPosition = FormStartPosition.CenterScreen;
        MinimumSize = new Size(920, 640);
        Size = new Size(1120, 760);
        FormBorderStyle = FormBorderStyle.Sizable;
        MaximizeBox = true;
        MinimizeBox = true;
        AutoScaleMode = AutoScaleMode.Dpi;
        Font = BaseFont;
        BackColor = ShellBackground;

        Controls.Add(BuildRootLayout());
        _installRoot.Text = options.InstallRoot;
        _launchEditor.Checked = options.LaunchAfterInstall || !options.NoDialog;
        _openToolWizard.Checked = options.OpenToolWizardAfterInstall;
        ApplyInitialOperation(options.Operation);
        _pages.Add(BuildWelcomePage());
        _pages.Add(BuildOptionsPage());
        _pages.Add(BuildReviewPage());
        _pages.Add(BuildProgressPage());
        _pages.Add(BuildResultPage());

        _backButton.Click += (_, _) => SetPage(_pageIndex - 1);
        _nextButton.Click += NextClicked;
        _cancelButton.Click += (_, _) => Close();
        _openLogFolderButton.Click += (_, _) => OpenLogFolder();
        _installRadio.CheckedChanged += (_, _) => StyleOperationCards();
        _repairRadio.CheckedChanged += (_, _) => StyleOperationCards();
        _uninstallRadio.CheckedChanged += (_, _) => StyleOperationCards();
        FormClosing += (_, eventArgs) =>
        {
            if (_operationRunning)
            {
                eventArgs.Cancel = true;
            }
        };
        StyleOperationCards();
        SetPage(0);
    }

    public int ExitCode { get; private set; }

    private Control BuildRootLayout()
    {
        TableLayoutPanel root = new()
        {
            Dock = DockStyle.Fill,
            ColumnCount = 1,
            RowCount = 2,
            BackColor = ShellBackground,
        };
        root.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
        root.RowStyles.Add(new RowStyle(SizeType.Absolute, 92));

        TableLayoutPanel shell = new()
        {
            Dock = DockStyle.Fill,
            ColumnCount = 2,
            RowCount = 1,
            BackColor = ShellBackground,
        };
        shell.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, RailWidth));
        shell.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        shell.Controls.Add(BuildRail(), 0, 0);
        shell.Controls.Add(_content, 1, 0);

        root.Controls.Add(shell, 0, 0);
        root.Controls.Add(BuildButtonBar(), 0, 1);
        return root;
    }

    private Control BuildRail()
    {
        TableLayoutPanel rail = new()
        {
            Dock = DockStyle.Fill,
            BackColor = RailBackground,
            Padding = new Padding(18, 18, 18, 18),
            ColumnCount = 1,
            RowCount = 6,
        };
        rail.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        rail.RowStyles.Add(new RowStyle(SizeType.Absolute, 46));
        rail.RowStyles.Add(new RowStyle(SizeType.Absolute, 34));
        rail.RowStyles.Add(new RowStyle(SizeType.Absolute, 40));
        rail.RowStyles.Add(new RowStyle(SizeType.Absolute, 220));
        rail.RowStyles.Add(new RowStyle(SizeType.Absolute, 92));
        rail.RowStyles.Add(new RowStyle(SizeType.Percent, 100));

        Label product = NewRailLabel("FOA-SDK Setup", 10.5f, FontStyle.Bold, Color.White);
        product.Dock = DockStyle.Fill;
        product.TextAlign = ContentAlignment.MiddleLeft;
        product.Margin = new Padding(0, 0, 0, 4);

        Label subtitle = NewRailLabel("Installer lifecycle", 8, FontStyle.Regular, RailMuted);
        subtitle.Dock = DockStyle.Fill;
        subtitle.TextAlign = ContentAlignment.TopLeft;
        subtitle.Margin = new Padding(0, 0, 0, 4);

        Label channel = new WrappingLabel()
        {
            Text = "MSI payload",
            Font = new Font(BaseFont.FontFamily, 7.5f, FontStyle.Bold),
            ForeColor = Color.FromArgb(52, 43, 23),
            BackColor = Accent,
            AutoSize = false,
            TextAlign = ContentAlignment.MiddleCenter,
            Dock = DockStyle.Fill,
            Margin = new Padding(0, 2, 0, 10),
            UseMnemonic = false,
        };

        string[] steps =
        {
            "1  Start",
            "2  Operation",
            "3  Review Package",
            "4  Run MSI Payload",
            "5  Finish",
        };

        TableLayoutPanel stepsPanel = new()
        {
            Dock = DockStyle.Fill,
            BackColor = RailBackground,
            ColumnCount = 1,
            RowCount = steps.Length,
            Margin = new Padding(0, 8, 0, 16),
        };
        stepsPanel.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        for (int index = 0; index < steps.Length; index++)
        {
            _stepLabels[index] = NewRailLabel(steps[index], 8, FontStyle.Bold, RailMuted);
            _stepLabels[index].Dock = DockStyle.Fill;
            _stepLabels[index].TextAlign = ContentAlignment.MiddleLeft;
            _stepLabels[index].Padding = new Padding(8, 0, 0, 0);
            _stepLabels[index].Margin = new Padding(0, 0, 0, 5);
            stepsPanel.RowStyles.Add(new RowStyle(SizeType.Percent, 20));
            stepsPanel.Controls.Add(_stepLabels[index], 0, index);
        }

        Label boundary = NewRailLabel(
            "External data untouched.",
            8,
            FontStyle.Regular,
            RailMuted);
        boundary.Dock = DockStyle.Fill;
        boundary.TextAlign = ContentAlignment.TopLeft;
        boundary.Margin = new Padding(0, 2, 0, 0);

        rail.Controls.Add(product, 0, 0);
        rail.Controls.Add(subtitle, 0, 1);
        rail.Controls.Add(channel, 0, 2);
        rail.Controls.Add(stepsPanel, 0, 3);
        rail.Controls.Add(boundary, 0, 4);
        return rail;
    }

    private Control BuildButtonBar()
    {
        FlowLayoutPanel bar = new()
        {
            Dock = DockStyle.Fill,
            FlowDirection = FlowDirection.RightToLeft,
            Padding = new Padding(16, 22, 28, 18),
            BackColor = Color.FromArgb(238, 243, 241),
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
        Panel page = BuildPage(
            "FOA-SDK Guided Installer",
            "Use this EXE for normal setup. It verifies the embedded MSI payload, runs Windows Installer for product-file changes, and can launch the installed SDK editor when setup completes.");

        TableLayoutPanel highlights = NewStack(3);
        highlights.Controls.Add(NewInfoCard(
            "EXE wizard function",
            "Readable guided flow for install, repair, uninstall, package review, logs, and post-install launch options."));
        highlights.Controls.Add(NewInfoCard(
            "MSI payload function",
            "Windows Installer owns product files, Start Menu registration, repair, upgrade, and uninstall."));
        highlights.Controls.Add(NewInfoCard(
            "Installed launcher",
            "FOA-SDK.exe is installed with the SDK and opens the editor with the bundled project."));
        AddPageContent(page, highlights);
        return page;
    }

    private Control BuildOptionsPage()
    {
        Panel page = BuildPage(
            "Choose an operation",
            "Select the Windows Installer lifecycle action and the per-user SDK location. Browse selects an existing folder; type a new path if setup should create it.");

        TableLayoutPanel choices = NewStack(4);
        _operationCards[0] = NewOperationCard(
            _installRadio,
            "Install a fresh reviewed SDK or upgrade an existing per-user installation.");
        _operationCards[1] = NewOperationCard(
            _repairRadio,
            "Restore product-owned files from the same reviewed MSI while preserving external workspace data.");
        _operationCards[2] = NewOperationCard(
            _uninstallRadio,
            "Remove installed product files and registration. External workspaces are never removed.");
        choices.Controls.Add(_operationCards[0]);
        choices.Controls.Add(_operationCards[1]);
        choices.Controls.Add(_operationCards[2]);

        Panel locationCard = NewCard(150);
        Label locationTitle = NewSectionTitle("Selected SDK install directory");
        locationTitle.Location = new Point(16, 12);
        Label locationHint = NewBodyLabel();
        locationHint.Text = "This is where product-owned SDK files go. It must be separate from game folders and external workspaces.";
        locationHint.Location = new Point(16, 36);
        locationHint.Size = new Size(PageContentMinWidth - 32, 44);
        FlowLayoutPanel locationRow = new()
        {
            FlowDirection = FlowDirection.LeftToRight,
            Location = new Point(16, 92),
            Size = new Size(PageContentMinWidth - 32, 34),
            WrapContents = false,
        };
        Button browse = NewSmallButton("Browse folder...");
        browse.Click += (_, _) => BrowseInstallRoot();
        locationRow.Controls.Add(_installRoot);
        locationRow.Controls.Add(browse);
        locationCard.Controls.Add(locationTitle);
        locationCard.Controls.Add(locationHint);
        locationCard.Controls.Add(locationRow);
        choices.Controls.Add(locationCard);
        AddPageContent(page, choices);
        return page;
    }

    private Control BuildReviewPage()
    {
        Panel page = BuildPage(
            "Ready to apply",
            "Review the package fingerprint and boundaries before Windows Installer changes product-owned files.");
        Panel reviewCard = NewCard(380);
        _reviewText.Location = new Point(18, 18);
        _reviewText.Size = new Size(PageContentMinWidth - 36, 320);
        reviewCard.Controls.Add(_reviewText);
        AddPageContent(page, reviewCard);
        return page;
    }

    private Control BuildProgressPage()
    {
        Panel page = BuildPage(
            "Applying FOA-SDK changes",
            "Windows Installer is processing the reviewed package. This window stays open until the operation returns a result.");
        Panel progressCard = NewCard(142);
        Label status = NewSectionTitle("Working on the reviewed MSI");
        status.Location = new Point(18, 18);
        _progress.Location = new Point(18, 66);
        _progress.Width = PageContentMinWidth - 36;
        progressCard.Controls.Add(status);
        progressCard.Controls.Add(_progress);
        AddPageContent(page, progressCard);
        return page;
    }

    private Control BuildResultPage()
    {
        Panel page = BuildPage("Setup result", string.Empty);
        Panel resultCard = NewCard(440);
        _resultStatus.Location = new Point(18, 18);
        _resultText.Location = new Point(18, 62);
        _resultText.Size = new Size(PageContentMinWidth - 36, 112);
        _resultLogLabel.Location = new Point(18, 190);
        _resultLogPath.Location = new Point(18, 222);
        _openLogFolderButton.Location = new Point(18, 262);
        _launchEditor.Location = new Point(18, 320);
        _openToolWizard.Location = new Point(18, 354);
        _resultLogPath.Visible = false;
        _resultLogLabel.Visible = false;
        _openLogFolderButton.Visible = false;
        _launchEditor.Visible = false;
        _openToolWizard.Visible = false;
        resultCard.Controls.Add(_resultStatus);
        resultCard.Controls.Add(_resultText);
        resultCard.Controls.Add(_resultLogLabel);
        resultCard.Controls.Add(_resultLogPath);
        resultCard.Controls.Add(_openLogFolderButton);
        resultCard.Controls.Add(_launchEditor);
        resultCard.Controls.Add(_openToolWizard);
        AddPageContent(page, resultCard);
        return page;
    }

    private static Panel BuildPage(string heading, string body)
    {
        Panel page = new() { Dock = DockStyle.Fill, BackColor = ShellBackground, AutoScroll = true };
        Label eyebrow = NewEyebrow("Tainted Grail: The Fall of Avalon Modding SDK");
        eyebrow.Location = new Point(0, 0);
        eyebrow.Size = new Size(PageContentMinWidth, 28);
        eyebrow.Name = "page-eyebrow";
        Label title = new WrappingLabel()
        {
            Text = heading,
            Font = new Font(BaseFont.FontFamily, 13.5f, FontStyle.Bold),
            ForeColor = Ink,
            AutoSize = false,
            Location = new Point(0, 34),
            Size = new Size(PageContentMinWidth, 52),
            UseMnemonic = false,
            Name = "page-title",
        };
        Label text = NewBodyLabel();
        text.Text = body;
        text.Location = new Point(0, 108);
        text.Size = new Size(PageContentMinWidth, 88);
        text.Name = "page-body";
        page.Controls.Add(eyebrow);
        page.Controls.Add(title);
        page.Controls.Add(text);
        page.Resize += (_, _) => ApplyResponsivePageLayout(page);
        return page;
    }

    private static void AddPageContent(Panel page, Control content)
    {
        content.Tag = PageContentTag;
        page.Controls.Add(content);
        ApplyResponsivePageLayout(page);
    }

    private static void ApplyResponsivePageLayout(Panel page)
    {
        int availableWidth = Math.Max(
            PageContentMinWidth,
            page.ClientSize.Width - SystemInformation.VerticalScrollBarWidth - 4);
        int width = Math.Min(PageContentMaxWidth, availableWidth);
        int top = 0;
        Label? eyebrow = page.Controls.Find("page-eyebrow", false).FirstOrDefault() as Label;
        Label? title = page.Controls.Find("page-title", false).FirstOrDefault() as Label;
        Label? body = page.Controls.Find("page-body", false).FirstOrDefault() as Label;

        if (eyebrow is not null)
        {
            eyebrow.SetBounds(0, top, width, MeasureLabelHeight(eyebrow, width, 24));
            top = eyebrow.Bottom + 8;
        }
        if (title is not null)
        {
            title.SetBounds(0, top, width, MeasureLabelHeight(title, width, 44));
            top = title.Bottom + 8;
        }
        if (body is not null && !string.IsNullOrWhiteSpace(body.Text))
        {
            body.SetBounds(0, top, width, MeasureLabelHeight(body, width, 58));
            top = body.Bottom + 28;
        }
        else
        {
            top += 18;
        }

        foreach (Control content in page.Controls.Cast<Control>().Where(control => Equals(control.Tag, PageContentTag)))
        {
            content.Location = new Point(0, top);
            content.Width = width;
            ApplyResponsiveChildLayout(content, width);
            top = content.Bottom + CardGap;
        }
    }

    private static void ApplyResponsiveChildLayout(Control content, int width)
    {
        if (content is TableLayoutPanel stack)
        {
            stack.Width = width;
            foreach (Control child in stack.Controls)
            {
                child.Width = width;
                ResizeCard(child, width);
            }
            stack.Height = stack.GetPreferredSize(new Size(width, 0)).Height;
            return;
        }
        ResizeCard(content, width);
    }

    private static void ResizeCard(Control card, int width)
    {
        card.Width = width;
        int maxBottom = 0;
        int nextTop = Math.Max(8, CardPadding - 2);
        foreach (Control child in card.Controls.Cast<Control>().OrderBy(control => control.Top).ThenBy(control => control.Left).ToList())
        {
            if (!child.Visible)
            {
                continue;
            }
            child.Top = nextTop;
            int childWidth = Math.Max(120, width - child.Left - CardPadding);
            switch (child)
            {
                case Label label:
                    label.Width = childWidth;
                    label.Height = MeasureLabelHeight(label, childWidth, label.AutoSize ? 22 : label.Height);
                    break;
                case RadioButton radio:
                    radio.Width = childWidth;
                    radio.Height = Math.Max(radio.Height, 24);
                    break;
                case CheckBox checkBox:
                    checkBox.Width = childWidth;
                    checkBox.Height = Math.Max(checkBox.Height, 24);
                    break;
                case ProgressBar progress:
                    progress.Width = childWidth;
                    break;
                case TextBox textBox:
                    textBox.Width = childWidth;
                    textBox.Height = Math.Max(textBox.Height, 30);
                    break;
                case FlowLayoutPanel row:
                    row.Width = childWidth;
                    row.Height = Math.Max(row.Height, 34);
                    TextBox? rowTextBox = row.Controls.OfType<TextBox>().FirstOrDefault();
                    Button? button = row.Controls.OfType<Button>().FirstOrDefault();
                    if (rowTextBox is not null && button is not null)
                    {
                        rowTextBox.Width = Math.Max(320, row.Width - button.Width - 20);
                    }
                    break;
            }
            maxBottom = Math.Max(maxBottom, child.Bottom);
            nextTop = child.Bottom + 8;
        }
        card.Height = Math.Max(card.MinimumSize.Height, maxBottom + CardPadding);
    }

    private static int MeasureLabelHeight(Label label, int width, int minimumHeight)
    {
        if (string.IsNullOrEmpty(label.Text))
        {
            return minimumHeight;
        }
        Size measured = TextRenderer.MeasureText(
            label.Text,
            label.Font,
            new Size(Math.Max(1, width), int.MaxValue),
            TextFormatFlags.WordBreak | TextFormatFlags.NoPrefix);
        return Math.Max(minimumHeight, measured.Height + 8);
    }

    private static TableLayoutPanel NewStack(int rows)
    {
        TableLayoutPanel stack = new()
        {
            ColumnCount = 1,
            RowCount = rows,
            Location = Point.Empty,
            Width = PageContentMinWidth,
            AutoSize = true,
            AutoSizeMode = AutoSizeMode.GrowAndShrink,
            BackColor = ShellBackground,
        };
        stack.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        for (int index = 0; index < rows; index++)
        {
            stack.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        }
        return stack;
    }

    private Panel NewOperationCard(RadioButton radio, string body)
    {
        Panel card = NewCard(112);
        radio.Location = new Point(16, 12);
        radio.Font = new Font(BaseFont, FontStyle.Bold);
        radio.ForeColor = Ink;
        Label description = NewBodyLabel();
        description.Text = body;
        description.Location = new Point(36, 42);
        description.Size = new Size(PageContentMinWidth - 72, 52);
        description.Cursor = Cursors.Hand;
        card.Cursor = Cursors.Hand;
        card.Click += (_, _) => radio.Checked = true;
        description.Click += (_, _) => radio.Checked = true;
        card.Controls.Add(radio);
        card.Controls.Add(description);
        return card;
    }

    private static Panel NewInfoCard(string heading, string body)
    {
        Panel card = NewCard(118);
        Label title = NewSectionTitle(heading);
        title.Location = new Point(16, 12);
        Label text = NewBodyLabel();
        text.Text = body;
        text.Location = new Point(16, 42);
        text.Size = new Size(PageContentMinWidth - 32, 58);
        card.Controls.Add(title);
        card.Controls.Add(text);
        return card;
    }

    private static Panel NewCard(int height)
    {
        Panel card = new()
        {
            Width = PageContentMinWidth,
            Height = height,
            MinimumSize = new Size(0, height),
            Margin = new Padding(0, 0, 0, 12),
            BackColor = Surface,
        };
        card.Paint += (_, eventArgs) =>
        {
            using Pen pen = new(Border);
            eventArgs.Graphics.DrawRectangle(pen, 0, 0, card.Width - 1, card.Height - 1);
        };
        return card;
    }

    private static Label NewEyebrow(string text) => new WrappingLabel()
    {
        Text = text,
        Font = new Font(BaseFont.FontFamily, 8.5f, FontStyle.Bold),
        ForeColor = Color.FromArgb(106, 92, 48),
        AutoSize = false,
        UseMnemonic = false,
    };

    private static Label NewSectionTitle(string text) => new WrappingLabel()
    {
        Text = text,
        Font = new Font(BaseFont, FontStyle.Bold),
        ForeColor = Ink,
        AutoSize = false,
        Height = 24,
        UseMnemonic = false,
    };

    private static Label NewBodyLabel() => new WrappingLabel()
    {
        AutoSize = false,
        Font = BaseFont,
        ForeColor = Muted,
        UseMnemonic = false,
    };

    private static Label NewStatusLabel() => new WrappingLabel()
    {
        AutoSize = false,
        Font = new Font(BaseFont, FontStyle.Bold),
        ForeColor = Ink,
        TextAlign = ContentAlignment.MiddleLeft,
        UseMnemonic = false,
        Size = new Size(PageContentMinWidth - 36, 36),
    };

    private static TextBox NewResultPathBox() => new()
    {
        ReadOnly = true,
        BorderStyle = BorderStyle.FixedSingle,
        BackColor = Color.White,
        ForeColor = Ink,
        Width = PageContentMinWidth - 36,
        Height = 30,
    };

    private static Label NewRailLabel(string text, float size, FontStyle style, Color color) => new WrappingLabel()
    {
        Text = text,
        Font = new Font(BaseFont.FontFamily, size, style),
        ForeColor = color,
        BackColor = RailBackground,
        AutoSize = false,
        UseMnemonic = false,
    };

    private static Button NewPrimaryButton(string text) => new()
    {
        Text = text,
        Width = 128,
        Height = 40,
        FlatStyle = FlatStyle.System,
    };

    private static Button NewNavButton(string text) => new()
    {
        Text = text,
        Width = 128,
        Height = 40,
        FlatStyle = FlatStyle.System,
    };

    private static Button NewSmallButton(string text) => new()
    {
        Text = text,
        Width = 140,
        Height = 32,
        FlatStyle = FlatStyle.System,
        Margin = new Padding(8, 0, 0, 0),
    };

    private void ApplyInitialOperation(InstallerOperation operation)
    {
        _installRadio.Checked = operation is InstallerOperation.InstallOrUpgrade;
        _repairRadio.Checked = operation is InstallerOperation.Repair;
        _uninstallRadio.Checked = operation is InstallerOperation.Uninstall;
    }

    private void StyleOperationCards()
    {
        StyleOperationCard(_operationCards[0], _installRadio.Checked);
        StyleOperationCard(_operationCards[1], _repairRadio.Checked);
        StyleOperationCard(_operationCards[2], _uninstallRadio.Checked);
    }

    private static void StyleOperationCard(Panel? card, bool selected)
    {
        if (card is null)
        {
            return;
        }
        card.BackColor = selected ? AccentSoft : Surface;
        card.Invalidate();
    }

    private void BrowseInstallRoot()
    {
        using FolderBrowserDialog dialog = new()
        {
            Description = "Select an existing folder to use as the SDK install directory. To use a new folder, type the full path in the install directory box.",
            InitialDirectory = Directory.Exists(_installRoot.Text)
                ? _installRoot.Text
                : Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            SelectedPath = Directory.Exists(_installRoot.Text)
                ? _installRoot.Text
                : Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            ShowNewFolderButton = false,
            UseDescriptionForTitle = true,
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
                    MessageBox.Show(this, ex.Message, "Unable to launch installed SDK editor", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    ExitCode = 1;
                    return;
                }
            }
            if (_operationSucceeded && _openToolWizard.Visible && _openToolWizard.Checked)
            {
                try
                {
                    ToolSetupWizardLauncher.Launch(_options.InstallRoot);
                }
                catch (Exception ex) when (ex is InvalidOperationException or System.ComponentModel.Win32Exception)
                {
                    MessageBox.Show(this, ex.Message, "Unable to open Tool Setup Wizard", MessageBoxButtons.OK, MessageBoxIcon.Error);
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
            LaunchAfterInstall = _launchEditor.Checked,
            OpenToolWizardAfterInstall = _openToolWizard.Checked,
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
        _reviewText.Text = $"Operation: {operation}{Environment.NewLine}{Environment.NewLine}"
            + $"Installation directory: {_options.InstallRoot}{Environment.NewLine}{Environment.NewLine}"
            + $"Payload: {_payload.MsiFile.Name}{Environment.NewLine}"
            + $"Reviewed MSI SHA-256: {_payload.Sha256}{Environment.NewLine}{Environment.NewLine}"
            + "FOA-SDK.exe will be the installed local launcher for the SDK editor and bundled project."
            + $"{Environment.NewLine}{Environment.NewLine}"
            + "External workspaces, generated content, FoA diagnostics, game files, saves, signing, and publication are outside this operation."
            + $"{Environment.NewLine}{Environment.NewLine}"
            + "Tool Setup Wizard: separate post-install path for local workspace and tool readiness.";
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
            _resultStatus.Text = result.Succeeded ? "Setup completed" : "Setup needs attention";
            _resultStatus.BackColor = result.Succeeded ? Color.FromArgb(224, 242, 232) : Color.FromArgb(254, 232, 222);
            _resultText.Text = result.Message;
            _resultLogPath.Text = result.LogPath;
            _resultLogLabel.Visible = true;
            _resultLogPath.Visible = true;
            _openLogFolderButton.Visible = true;
            _launchEditor.Visible = result.Succeeded && _options.Operation is not InstallerOperation.Uninstall;
            _openToolWizard.Visible = result.Succeeded && _options.Operation is not InstallerOperation.Uninstall;
        }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException or InvalidOperationException or System.ComponentModel.Win32Exception)
        {
            _operationSucceeded = false;
            ExitCode = 1;
            _resultStatus.Text = "Setup failed before completion";
            _resultStatus.BackColor = Color.FromArgb(254, 232, 222);
            _resultText.Text = $"Setup failed before completion.{Environment.NewLine}{Environment.NewLine}{ex.Message}";
            _resultLogPath.Clear();
            _resultLogLabel.Visible = false;
            _resultLogPath.Visible = false;
            _openLogFolderButton.Visible = false;
            _launchEditor.Visible = false;
            _openToolWizard.Visible = false;
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
        if (_pages[index] is Panel page)
        {
            ApplyResponsivePageLayout(page);
        }
        _backButton.Enabled = !_operationRunning && index is 1 or 2;
        _cancelButton.Enabled = !_operationRunning && index != 4;
        _nextButton.Enabled = !_operationRunning && index != 3;
        _nextButton.Text = index switch
        {
            2 => "Apply",
            4 => "Finish",
            _ => "Next >",
        };
        UpdateStepRail(index);
    }

    private void UpdateStepRail(int activeIndex)
    {
        for (int index = 0; index < _stepLabels.Length; index++)
        {
            Label? label = _stepLabels[index];
            if (label is null)
            {
                continue;
            }
            bool active = index == activeIndex;
            label.ForeColor = active ? Color.White : RailMuted;
            label.BackColor = active ? Color.FromArgb(54, 73, 67) : RailBackground;
        }
    }

    private void OpenLogFolder()
    {
        string logPath = _resultLogPath.Text;
        string? directory = string.IsNullOrWhiteSpace(logPath)
            ? null
            : Path.GetDirectoryName(logPath);
        if (directory is null || !Directory.Exists(directory))
        {
            MessageBox.Show(this, "The MSI log folder is not available yet.", "Open log folder", MessageBoxButtons.OK, MessageBoxIcon.Information);
            return;
        }
        try
        {
            Process.Start(new ProcessStartInfo
            {
                FileName = directory,
                UseShellExecute = true,
            });
        }
        catch (Exception ex) when (ex is InvalidOperationException or System.ComponentModel.Win32Exception)
        {
            MessageBox.Show(this, ex.Message, "Unable to open log folder", MessageBoxButtons.OK, MessageBoxIcon.Error);
        }
    }
}
