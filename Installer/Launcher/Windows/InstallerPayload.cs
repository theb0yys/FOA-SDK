// SPDX-License-Identifier: Apache-2.0 OR MIT
using System.Reflection;
using System.Security.Cryptography;
using System.Text;

namespace FOA.SDK.InstallerLauncher;

internal sealed class InstallerPayload : IDisposable
{
    private const string EmbeddedMsiName = "FOA.SDK.Payload.msi";
    private const string EmbeddedChecksumName = "FOA.SDK.Payload.msi.sha256";
    private readonly DirectoryInfo? _temporaryRoot;

    private InstallerPayload(FileInfo msiFile, string sha256, DirectoryInfo? temporaryRoot)
    {
        MsiFile = msiFile;
        Sha256 = sha256;
        _temporaryRoot = temporaryRoot;
    }

    public FileInfo MsiFile { get; }
    public string Sha256 { get; }

    public static InstallerPayload Resolve(string? explicitPath)
    {
        if (!string.IsNullOrWhiteSpace(explicitPath))
        {
            return FromExternalFile(new FileInfo(Path.GetFullPath(
                Environment.ExpandEnvironmentVariables(explicitPath))));
        }

        Assembly assembly = Assembly.GetExecutingAssembly();
        if (assembly.GetManifestResourceInfo(EmbeddedMsiName) is not null)
        {
            return FromEmbeddedResource(assembly);
        }

        FileInfo[] adjacent = new DirectoryInfo(AppContext.BaseDirectory).GetFiles("*.msi", SearchOption.TopDirectoryOnly);
        if (adjacent.Length != 1)
        {
            throw new InvalidOperationException(
                $"The installer requires one embedded MSI or exactly one reviewed MSI beside the EXE; found {adjacent.Length}.");
        }
        return FromExternalFile(adjacent[0]);
    }

    public void Dispose()
    {
        if (_temporaryRoot is null)
        {
            return;
        }
        try
        {
            _temporaryRoot.Delete(recursive: true);
        }
        catch (IOException)
        {
            // Windows Installer may retain a short-lived read handle; the private temporary root is safe to leave for OS cleanup.
        }
        catch (UnauthorizedAccessException)
        {
            // The payload remains confined to the private temporary root and will be cleaned by the operating system.
        }
    }

    private static InstallerPayload FromExternalFile(FileInfo msi)
    {
        RequireRegularMsi(msi);
        FileInfo checksum = new(msi.FullName + ".sha256");
        if (!checksum.Exists)
        {
            throw new InvalidOperationException($"The reviewed MSI checksum sidecar is missing: {checksum.FullName}");
        }
        string expected = ParseChecksum(File.ReadAllText(checksum.FullName, Encoding.UTF8), msi.Name);
        DirectoryInfo temporaryRoot = CreateTemporaryRoot();
        FileInfo captured = new(Path.Combine(temporaryRoot.FullName, "FOA-SDK-Payload.msi"));
        try
        {
            using (FileStream input = new(msi.FullName, FileMode.Open, FileAccess.Read, FileShare.Read))
            using (FileStream output = new(captured.FullName, FileMode.CreateNew, FileAccess.Write, FileShare.None))
            {
                input.CopyTo(output);
                output.Flush(flushToDisk: true);
            }
            VerifyChecksum(captured, expected);
            return new InstallerPayload(captured, expected, temporaryRoot);
        }
        catch
        {
            temporaryRoot.Delete(recursive: true);
            throw;
        }
    }

    private static InstallerPayload FromEmbeddedResource(Assembly assembly)
    {
        using Stream msiStream = assembly.GetManifestResourceStream(EmbeddedMsiName)
            ?? throw new InvalidOperationException("The embedded reviewed MSI is missing.");
        using Stream checksumStream = assembly.GetManifestResourceStream(EmbeddedChecksumName)
            ?? throw new InvalidOperationException("The embedded MSI checksum is missing.");
        using StreamReader checksumReader = new(checksumStream, Encoding.UTF8, detectEncodingFromByteOrderMarks: true);
        string expected = ParseChecksum(checksumReader.ReadToEnd(), expectedFileName: null);

        DirectoryInfo temporaryRoot = CreateTemporaryRoot();
        FileInfo extracted = new(Path.Combine(temporaryRoot.FullName, "FOA-SDK-Payload.msi"));
        try
        {
            using (FileStream output = new(extracted.FullName, FileMode.CreateNew, FileAccess.Write, FileShare.None))
            {
                msiStream.CopyTo(output);
                output.Flush(flushToDisk: true);
            }
            VerifyChecksum(extracted, expected);
            return new InstallerPayload(extracted, expected, temporaryRoot);
        }
        catch
        {
            temporaryRoot.Delete(recursive: true);
            throw;
        }
    }

    private static void RequireRegularMsi(FileInfo msi)
    {
        msi.Refresh();
        if (!msi.Exists || !string.Equals(msi.Extension, ".msi", StringComparison.OrdinalIgnoreCase))
        {
            throw new InvalidOperationException($"The installer payload is missing or is not an MSI: {msi.FullName}");
        }
        if ((msi.Attributes & FileAttributes.ReparsePoint) != 0)
        {
            throw new InvalidOperationException($"The installer MSI must not be a symbolic link or reparse point: {msi.FullName}");
        }
    }

    private static string ParseChecksum(string text, string? expectedFileName)
    {
        string[] parts = text.Trim().Split((char[]?)null, StringSplitOptions.RemoveEmptyEntries);
        if (parts.Length is < 1 or > 2
            || parts[0].Length != 64
            || parts[0].Any(character => !Uri.IsHexDigit(character))
            || parts[0] != parts[0].ToLowerInvariant())
        {
            throw new InvalidOperationException("The MSI checksum sidecar is not a canonical lowercase SHA-256 record.");
        }
        if (expectedFileName is not null
            && parts.Length == 2
            && !string.Equals(Path.GetFileName(parts[1]), expectedFileName, StringComparison.OrdinalIgnoreCase))
        {
            throw new InvalidOperationException("The MSI checksum sidecar names a different installer package.");
        }
        return parts[0];
    }

    private static void VerifyChecksum(FileInfo msi, string expected)
    {
        using FileStream input = new(msi.FullName, FileMode.Open, FileAccess.Read, FileShare.Read);
        string actual = Convert.ToHexString(SHA256.HashData(input)).ToLowerInvariant();
        if (!string.Equals(actual, expected, StringComparison.Ordinal))
        {
            throw new InvalidOperationException("The MSI SHA-256 does not match its reviewed checksum.");
        }
    }

    private static DirectoryInfo CreateTemporaryRoot() => Directory.CreateDirectory(Path.Combine(
        Path.GetTempPath(),
        "FOA-SDK-Installer",
        Guid.NewGuid().ToString("N")));
}
