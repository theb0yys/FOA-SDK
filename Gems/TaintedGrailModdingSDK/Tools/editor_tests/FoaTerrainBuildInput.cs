// Copyright (c) Contributors to the Open 3D Engine Project.
// For complete copyright and license terms please see the LICENSE at the root of this distribution.
// SPDX-License-Identifier: Apache-2.0 OR MIT
// Neutral input reader for the bounded M6 profile. Expected fingerprints come from the
// caller's reviewed input binding, never from this file. A hash is not authorization.
using System;
using System.IO;
using System.Security.Cryptography;
using System.Text;

public sealed class FoaTerrainBuildInput
{
    public const int MaximumDocumentBytes = 65536;
    public const int SampleBytes = 2178;
    public const int MaximumBytes = 144 + MaximumDocumentBytes + SampleBytes;
    readonly byte[] encoded;
    public readonly byte[] Samples;
    public byte[] GetEncodedBytes() { return (byte[])encoded.Clone(); }
    public readonly string CanonicalDocument, DocumentFingerprint, InputFingerprint;

    FoaTerrainBuildInput(byte[] bytes, byte[] samples, string document, string documentFingerprint, string inputFingerprint)
    {
        encoded = (byte[])bytes.Clone();
        Samples = samples; CanonicalDocument = document;
        DocumentFingerprint = documentFingerprint; InputFingerprint = inputFingerprint;
    }
    static void Require(bool condition, string message)
    {
        if (!condition) throw new InvalidDataException(message);
    }
    public static string Fingerprint(byte[] bytes)
    {
        using (var sha = SHA256.Create())
            return "sha256:" + BitConverter.ToString(sha.ComputeHash(bytes)).Replace("-", "").ToLowerInvariant();
    }
    static bool IsFingerprint(string value)
    {
        if (value == null || value.Length != 71 || !value.StartsWith("sha256:", StringComparison.Ordinal)) return false;
        for (int i = 7; i < value.Length; ++i)
            if (!((value[i] >= '0' && value[i] <= '9') || (value[i] >= 'a' && value[i] <= 'f'))) return false;
        return true;
    }
    public static FoaTerrainBuildInput Decode(byte[] bytes, string expectedInputFingerprint, string expectedDocumentFingerprint)
    {
        Require(bytes != null && bytes.Length >= 144 + SampleBytes + 1 && bytes.Length <= MaximumBytes, "Native input size is invalid.");
        Require(IsFingerprint(expectedInputFingerprint) && IsFingerprint(expectedDocumentFingerprint), "Explicit native input bindings are required.");
        Require(Fingerprint(bytes) == expectedInputFingerprint, "Native input changed since its build preview.");
        using (var reader = new BinaryReader(new MemoryStream(bytes, false), Encoding.UTF8))
        {
            Require(Encoding.ASCII.GetString(reader.ReadBytes(8)) == "FOAHM001", "Unsupported native terrain input version.");
            uint documentSize = reader.ReadUInt32(), sampleSize = reader.ReadUInt32();
            Require(documentSize > 0 && documentSize <= MaximumDocumentBytes && sampleSize == SampleBytes &&
                    bytes.Length == 144L + documentSize + sampleSize, "Native input lengths or trailing bytes are invalid.");
            string documentFingerprint = "sha256:" + Encoding.ASCII.GetString(reader.ReadBytes(64));
            string samplesFingerprint = "sha256:" + Encoding.ASCII.GetString(reader.ReadBytes(64));
            Require(IsFingerprint(documentFingerprint) && IsFingerprint(samplesFingerprint) && documentFingerprint == expectedDocumentFingerprint,
                    "Native input belongs to another source revision.");
            byte[] documentBytes = reader.ReadBytes((int)documentSize);
            Require(Fingerprint(documentBytes) == documentFingerprint, "Canonical document digest differs.");
            string document = new UTF8Encoding(false, true).GetString(documentBytes);
            // The canonical body remains opaque provenance. Core owns full semantic validation;
            // this reader checks framing and the externally supplied exact document binding.
            Require(document.StartsWith("{\"schema\":\"foa.terrain-heightmap\",\"schema_version\":1,", StringComparison.Ordinal) &&
                    document.EndsWith("}", StringComparison.Ordinal), "Unsupported canonical source envelope.");
            byte[] samples = reader.ReadBytes((int)sampleSize);
            Require(Fingerprint(samples) == samplesFingerprint, "Native input samples differ from their digest.");
            return new FoaTerrainBuildInput(bytes, samples, document, documentFingerprint, expectedInputFingerprint);
        }
    }
    public static FoaTerrainBuildInput Read(string path, string expectedInputFingerprint, string expectedDocumentFingerprint)
    {
        Require(!String.IsNullOrEmpty(path) && Path.IsPathRooted(path), "Absolute native input path required.");
        var info = new FileInfo(path);
        Require(info.Exists && (info.Attributes & FileAttributes.ReparsePoint) == 0, "Missing or linked native input.");
        for (var parent = info.Directory; parent != null; parent = parent.Parent)
            Require((parent.Attributes & FileAttributes.ReparsePoint) == 0, "Linked native input ancestor.");
        using (var file = new FileStream(info.FullName, FileMode.Open, FileAccess.Read, FileShare.Read))
        {
            Require(file.Length > 0 && file.Length <= MaximumBytes, "Native input exceeds its read budget.");
            var bytes = new byte[(int)file.Length];
            int offset = 0;
            while (offset < bytes.Length)
            {
                int count = file.Read(bytes, offset, bytes.Length - offset);
                Require(count > 0, "Native input was truncated while reading.");
                offset += count;
            }
            Require(file.ReadByte() == -1, "Native input grew while reading.");
            return Decode(bytes, expectedInputFingerprint, expectedDocumentFingerprint);
        }
    }
}
