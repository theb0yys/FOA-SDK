// Copyright (c) Contributors to the Open 3D Engine Project.
// For complete copyright and license terms please see the LICENSE at the root of this distribution.
// SPDX-License-Identifier: Apache-2.0 OR MIT
// Synthetic Unity authoring qualification only. Copy this source into Assets/Editor in a fresh
// private project. This is not a Framework provider, canonical importer, or game runtime adapter.
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Security.Cryptography;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using UnityEngine.SceneManagement;
using Debug = UnityEngine.Debug;

public static class FoaHeightmapNativeProbe
{
    static FoaTerrainBuildInput coreInput;
    const int Resolution = 33;
    const float HeightRange = 8f;
    const float MinimumHeight = -2f;
    // Declared before measurement: at most two U16 codes plus float arithmetic error.
    const double NormalizedTolerance = 2.0 / 65535.0 + 0.0000001;
    const string FixtureRoot = "Assets/FoaM6Fixture";
    const string DataPath = FixtureRoot + "/terrain.asset";
    const string PrefabPath = FixtureRoot + "/terrain.prefab";
    const string BundleName = "foa-m6-terrain";
    const string ExactEditor = "6000.0.64f1";

    [Serializable] public class Observation
    {
        public string stage, decodedSha256;
        public int comparedSamples, collisionProbes;
        public double maxNormalizedError, maxElevationErrorMetres, maxCollisionErrorMetres;
        public float[] cornerHeights;
        public Vector3 size, origin;
    }
    [Serializable] public class Artifact
    {
        public string path, sha256;
        public long bytes;
    }
    [Serializable] public class Report
    {
        public string schema = "foa.m6.native-heightmap-qualification";
        public int schemaVersion = 1;
        public string status = "FAILED", unityVersion, error, sourceSha256, terrainGuid, prefabGuid;
        public string sourceKind = "sdk-fixed", inputFingerprint, sourceDocumentFingerprint;
        public string sourceCoordinates = "right-handed; +X east; +Y north; +Z up; row zero north; grid-vertex";
        public string nativeTransform = "Unity (x,y,z) = canonical (x,z,y); reverse source rows";
        public string losses = "U16 normalization and native height quantization; no resampling";
        public double normalizedTolerance = NormalizedTolerance, elapsedMilliseconds;
        public int width = Resolution, height = Resolution, rejectionChecks, coreInputRejectionChecks;
        public float sampleSpacingMetres = 1f, minimumHeightMetres = MinimumHeight, maximumHeightMetres = MinimumHeight + HeightRange;
        public bool repeatedBundleBytesEqual;
        // Candidate observations never authorize another operation.
        public bool runtimeUseAllowed, deploymentAllowed, publicationAllowed, packagingAllowed, gameWriteAllowed, evidencePromotionAllowed;
        public List<Observation> observations = new List<Observation>();
        public List<Artifact> artifacts = new List<Artifact>();
    }

    // Not an import API: the only input is this SDK-owned asymmetric fixture.
    static ushort SourceSample(int column, int row)
    {
        if (coreInput != null)
        {
            int offset = 2 * (row * Resolution + column);
            return (ushort)(coreInput.Samples[offset] | coreInput.Samples[offset + 1] << 8);
        }
        if (column == 0 && row == 0) return 0;
        if (column == 32 && row == 0) return 16384;
        if (column == 0 && row == 32) return 49151;
        if (column == 32 && row == 32) return 65535;
        return (ushort)((column * 977 + row * 331 + column * row * 17 + 123) % 65536);
    }
    static byte[] SourceBytes()
    {
        var bytes = new byte[Resolution * Resolution * 2];
        for (int row = 0; row < Resolution; ++row)
            for (int column = 0; column < Resolution; ++column)
            {
                ushort value = SourceSample(column, row);
                int offset = 2 * (row * Resolution + column);
                bytes[offset] = (byte)value;
                bytes[offset + 1] = (byte)(value >> 8);
            }
        return bytes;
    }
    static float[,] Decode(byte[] bytes)
    {
        if (bytes == null || bytes.Length != Resolution * Resolution * 2)
            throw new InvalidDataException("The fixed U16LE fixture must contain exactly 1089 samples.");
        var heights = new float[Resolution, Resolution];
        for (int row = 0; row < Resolution; ++row)
            for (int column = 0; column < Resolution; ++column)
            {
                int offset = 2 * (row * Resolution + column);
                heights[Resolution - 1 - row, column] = (bytes[offset] | bytes[offset + 1] << 8) / 65535f;
            }
        return heights;
    }
    static string Hash(byte[] bytes)
    {
        using (var sha = SHA256.Create()) return BitConverter.ToString(sha.ComputeHash(bytes)).Replace("-", "").ToLowerInvariant();
    }
    static void Require(bool condition, string message)
    {
        if (!condition) throw new InvalidDataException(message);
    }
    static string PrivateDirectory(string path)
    {
        Require(!String.IsNullOrEmpty(path) && Path.IsPathRooted(path) && Directory.Exists(path), "Existing absolute private directory required.");
        string full = Path.GetFullPath(path);
        for (var parent = new DirectoryInfo(full); parent != null; parent = parent.Parent)
        {
            Require((parent.Attributes & FileAttributes.ReparsePoint) == 0, "Reparse directory refused.");
            Require(!Directory.Exists(Path.Combine(parent.FullName, ".git")) && !File.Exists(Path.Combine(parent.FullName, ".git")), "Native outputs must remain outside source checkouts.");
        }
        return full;
    }
    static string CheckContext()
    {
        Require(Application.unityVersion == ExactEditor, "The exact qualification Editor is required.");
        Require(Application.isBatchMode, "Run this qualification in batch mode.");
        string project = PrivateDirectory(Path.GetDirectoryName(Application.dataPath));
        string output = PrivateDirectory(Environment.GetEnvironmentVariable("FOA_HEIGHTMAP_OUTPUT"));
        Require(!output.StartsWith(project + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase) && output != project,
            "Use a separate private output directory.");
        var scripts = Directory.GetFiles(Application.dataPath, "*.cs", SearchOption.AllDirectories);
        Require(scripts.Length == 2, "The disposable project must contain only the probe and neutral input reader.");
        Array.Sort(scripts, StringComparer.Ordinal);
        Require(Path.GetFileName(scripts[0]) == "FoaHeightmapNativeProbe.cs" && Path.GetFileName(scripts[1]) == "FoaTerrainBuildInput.cs",
            "Unexpected qualification script inventory.");
        return output;
    }
    static void WriteNew(string path, byte[] bytes)
    {
        using (var file = new FileStream(path, FileMode.CreateNew, FileAccess.Write, FileShare.None)) file.Write(bytes, 0, bytes.Length);
    }
    static Artifact Describe(string root, string path)
    {
        var file = new FileInfo(path);
        Require(file.Length > 0 && file.Length <= 64 * 1024 * 1024 && (file.Attributes & FileAttributes.ReparsePoint) == 0,
            "Missing, linked or oversized fixture artifact.");
        return new Artifact { path = path.Substring(root.Length + 1).Replace('\\', '/'), bytes = file.Length, sha256 = Hash(File.ReadAllBytes(path)) };
    }
    static Observation Compare(TerrainData data, string stage, string output)
    {
        Require(data != null && data.heightmapResolution == Resolution, "Native heightmap resolution changed.");
        Require(data.size == new Vector3(32, HeightRange, 32), "Native terrain extent changed.");
        var actual = data.GetHeights(0, 0, Resolution, Resolution);
        var observation = new Observation { stage = stage, size = data.size, origin = new Vector3(0, MinimumHeight, 0) };
        // Independently calculate expected native coordinates; do not compare with Decode's output.
        using (var stream = new MemoryStream())
        using (var writer = new BinaryWriter(stream))
        {
            for (int z = 0; z < Resolution; ++z)
                for (int x = 0; x < Resolution; ++x)
                {
                    double expected = SourceSample(x, 32 - z) / 65535.0;
                    double error = Math.Abs(actual[z, x] - expected);
                    Require(!Double.IsNaN(error) && error <= NormalizedTolerance, "Height/orientation mismatch at " + x + "," + z);
                    observation.maxNormalizedError = Math.Max(observation.maxNormalizedError, error);
                    double elevationError = Math.Abs(data.GetHeight(x, z) - expected * HeightRange);
                    Require(elevationError <= NormalizedTolerance * HeightRange, "Native metre conversion mismatch.");
                    observation.maxElevationErrorMetres = Math.Max(observation.maxElevationErrorMetres, elevationError);
                    writer.Write(actual[z, x]);
                    ++observation.comparedSamples;
                }
            var bytes = stream.ToArray();
            observation.decodedSha256 = Hash(bytes);
            WriteNew(Path.Combine(output, stage + ".f32le"), bytes);
        }
        observation.cornerHeights = new[] { actual[0, 0], actual[0, 32], actual[32, 0], actual[32, 32] };
        return observation;
    }
    static void CheckCollision(GameObject instance, Observation observation)
    {
        Require(instance != null && instance.transform.position == new Vector3(0, MinimumHeight, 0), "Native instance origin changed.");
        var terrain = instance.GetComponent<Terrain>();
        var collider = instance.GetComponent<TerrainCollider>();
        Require(terrain != null && collider != null && collider.terrainData == terrain.terrainData, "Terrain collider binding lost.");
        Physics.SyncTransforms();
        int[,] points = { { 1, 1 }, { 7, 23 }, { 15, 19 }, { 30, 29 }, { 16, 16 } };
        for (int i = 0; i < points.GetLength(0); ++i)
        {
            int x = points[i, 0], z = points[i, 1];
            RaycastHit hit;
            Require(collider.Raycast(new Ray(new Vector3(x, 20, z), Vector3.down), out hit, 32), "Terrain collision probe missed.");
            double expected = MinimumHeight + SourceSample(x, 32 - z) / 65535.0 * HeightRange;
            double error = Math.Abs(hit.point.y - expected);
            Require(error <= NormalizedTolerance * HeightRange + 0.00001, "Terrain collision height mismatch.");
            observation.maxCollisionErrorMetres = Math.Max(observation.maxCollisionErrorMetres, error);
            ++observation.collisionProbes;
        }
    }
    static void Rejections(Report report)
    {
        foreach (var bytes in new byte[][] { null, new byte[0], new byte[2177], new byte[2179] })
        {
            bool rejected = false;
            try { Decode(bytes); } catch (InvalidDataException) { rejected = true; }
            Require(rejected, "Malformed fixture was accepted.");
            ++report.rejectionChecks;
        }
    }
    static void CoreInputRejections(Report report)
    {
        byte[] original = coreInput.GetEncodedBytes();
        Action<byte[], string, string> reject = (data, inputFingerprint, documentFingerprint) =>
        {
            bool rejected = false;
            try { FoaTerrainBuildInput.Decode(data, inputFingerprint, documentFingerprint); }
            catch (InvalidDataException) { rejected = true; }
            Require(rejected, "Malformed or stale native handoff was accepted.");
            ++report.coreInputRejectionChecks;
        };
        reject(new byte[0], coreInput.InputFingerprint, coreInput.DocumentFingerprint);
        reject(new byte[FoaTerrainBuildInput.MaximumBytes + 1], coreInput.InputFingerprint, coreInput.DocumentFingerprint);
        reject(original, "sha256:" + new string('0', 64), coreInput.DocumentFingerprint);
        reject(original, coreInput.InputFingerprint, "sha256:" + new string('0', 64));
        // Recompute the outer checksum to test the inner framing and digest guards themselves.
        foreach (int offset in new[] { 7, 8, 12, 16, 80, 144, original.Length - 1 })
        {
            byte[] changed = (byte[])original.Clone();
            changed[offset] ^= 1;
            reject(changed, FoaTerrainBuildInput.Fingerprint(changed), coreInput.DocumentFingerprint);
        }
        byte[] trailing = new byte[original.Length + 1];
        Array.Copy(original, trailing, original.Length);
        reject(trailing, FoaTerrainBuildInput.Fingerprint(trailing), coreInput.DocumentFingerprint);
    }
    static string Build(string output, string name, Report report)
    {
        string folder = Path.Combine(output, name);
        Require(!Directory.Exists(folder), "Build output already exists.");
        Directory.CreateDirectory(folder);
        var map = new AssetBundleBuild { assetBundleName = BundleName, assetNames = new[] { DataPath, PrefabPath } };
        var manifest = BuildPipeline.BuildAssetBundles(folder, new[] { map },
            BuildAssetBundleOptions.ForceRebuildAssetBundle | BuildAssetBundleOptions.ChunkBasedCompression | BuildAssetBundleOptions.StrictMode,
            BuildTarget.StandaloneWindows64);
        Require(manifest != null && manifest.GetAllAssetBundles().Length == 1, "Native bundle build failed or produced an unexpected inventory.");
        foreach (string path in Directory.GetFiles(folder)) report.artifacts.Add(Describe(output, path));
        return Path.Combine(folder, BundleName);
    }
    static void CheckBundle(string bundlePath, string stage, string output, Report report)
    {
        var bundle = AssetBundle.LoadFromFile(bundlePath);
        Require(bundle != null, "Native bundle could not be reopened.");
        GameObject instance = null;
        try
        {
            Require(bundle.GetAllAssetNames().Length == 2, "Unexpected native asset inventory.");
            var data = bundle.LoadAsset<TerrainData>(DataPath);
            var prefab = bundle.LoadAsset<GameObject>(PrefabPath);
            Require(prefab != null && prefab.GetComponent<Terrain>() != null && prefab.GetComponent<Terrain>().terrainData == data,
                "Bundled prefab lost its exact terrain reference.");
            var observation = Compare(data, stage, output);
            instance = UnityEngine.Object.Instantiate(prefab);
            CheckCollision(instance, observation);
            report.observations.Add(observation);
        }
        finally
        {
            if (instance != null) UnityEngine.Object.DestroyImmediate(instance);
            bundle.Unload(true);
        }
    }
    public static void Run() { Execute(false); }
    public static void Reopen() { Execute(true); }
    public static void RunCore() { Execute(false, true); }
    public static void ReopenCore() { Execute(true, true); }
    static void Execute(bool reopen, bool fromCore = false)
    {
        string output = null;
        bool mayWriteReport = false;
        var report = new Report { unityVersion = Application.unityVersion };
        var clock = Stopwatch.StartNew();
        string reportName = reopen ? "reopen.json" : "result.json";
        try
        {
            output = CheckContext();
            Require(!File.Exists(Path.Combine(output, reportName)), "This attempt already has a report; use a fresh output root.");
            if (!reopen) Require(Directory.GetFileSystemEntries(output).Length == 0, "Initial output must be empty.");
            mayWriteReport = true;
            coreInput = fromCore ? FoaTerrainBuildInput.Read(
                Environment.GetEnvironmentVariable("FOA_HEIGHTMAP_INPUT"),
                Environment.GetEnvironmentVariable("FOA_HEIGHTMAP_INPUT_FINGERPRINT"),
                Environment.GetEnvironmentVariable("FOA_HEIGHTMAP_DOCUMENT_FINGERPRINT")) : null;
            if (coreInput != null)
            {
                report.sourceKind = "core-terrain-handoff";
                report.inputFingerprint = coreInput.InputFingerprint;
                report.sourceDocumentFingerprint = coreInput.DocumentFingerprint;
                CoreInputRejections(report);
            }
            Rejections(report);
            report.sourceSha256 = Hash(SourceBytes());
            if (reopen)
            {
                Require(Hash(File.ReadAllBytes(Path.Combine(output, "source.u16le"))) == report.sourceSha256, "Source fixture changed.");
                CheckBundle(Path.Combine(output, "build-a", BundleName), "fresh-process", output, report);
            }
            else
            {
                Require(!Directory.Exists(FixtureRoot), "Native fixture already exists.");
                Require(AssetDatabase.GetAllAssetPaths().Length < 100, "A fresh empty qualification project is required.");
                WriteNew(Path.Combine(output, "source.u16le"), SourceBytes());
                Directory.CreateDirectory(FixtureRoot);
                AssetDatabase.Refresh(ImportAssetOptions.ForceSynchronousImport);
                var data = new TerrainData { heightmapResolution = Resolution, size = new Vector3(32, HeightRange, 32) };
                Require(data.heightmapResolution == Resolution, "Unity silently changed the requested resolution.");
                data.SetHeights(0, 0, Decode(File.ReadAllBytes(Path.Combine(output, "source.u16le"))));
                AssetDatabase.CreateAsset(data, DataPath);
                var scene = EditorSceneManager.NewScene(NewSceneSetup.EmptyScene, NewSceneMode.Single);
                var terrain = Terrain.CreateTerrainGameObject(data);
                terrain.name = "FOA M6 Synthetic Terrain";
                terrain.transform.position = new Vector3(0, MinimumHeight, 0);
                Require(PrefabUtility.SaveAsPrefabAsset(terrain, PrefabPath) != null, "Native prefab save failed.");
                Require(EditorSceneManager.SaveScene(scene, FixtureRoot + "/fixture.unity"), "Native scene save failed.");
                AssetDatabase.SaveAssets();
                var first = Compare(data, "created", output);
                CheckCollision(terrain, first);
                report.observations.Add(first);
                EditorSceneManager.NewScene(NewSceneSetup.EmptyScene, NewSceneMode.Single);
                Resources.UnloadAsset(data);
                data = AssetDatabase.LoadAssetAtPath<TerrainData>(DataPath);
                report.observations.Add(Compare(data, "asset-reopened", output));
                report.terrainGuid = AssetDatabase.AssetPathToGUID(DataPath);
                report.prefabGuid = AssetDatabase.AssetPathToGUID(PrefabPath);
                Require(report.terrainGuid.Length == 32 && report.prefabGuid.Length == 32 && report.terrainGuid != report.prefabGuid, "Unity native GUID binding failed.");
                string firstBuild = Build(output, "build-a", report);
                CheckBundle(firstBuild, "bundle-a", output, report);
                string secondBuild = Build(output, "build-b", report);
                CheckBundle(secondBuild, "bundle-b", output, report);
                report.repeatedBundleBytesEqual = Hash(File.ReadAllBytes(firstBuild)) == Hash(File.ReadAllBytes(secondBuild));
            }
            report.status = "PASSED";
        }
        catch (Exception error) { report.error = error.ToString(); Debug.LogError(report.error); }
        finally
        {
            report.elapsedMilliseconds = clock.Elapsed.TotalMilliseconds;
            if (mayWriteReport) WriteNew(Path.Combine(output, reportName), System.Text.Encoding.UTF8.GetBytes(JsonUtility.ToJson(report, true)));
            EditorApplication.Exit(report.status == "PASSED" ? 0 : 1);
        }
    }
}
