// Copyright (c) Contributors to the Open 3D Engine Project.
// For complete copyright and license terms please see the LICENSE at the root of this distribution.
// SPDX-License-Identifier: Apache-2.0 OR MIT
// Private, isolated Unity qualification. Inputs/outputs are never game installation writes.
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Security.Cryptography;
using UnityEditor;
using UnityEngine;

public static class FoaLightMigrationProbe
{
    [Serializable] public class Input
    {
        public string name, nativeJson;
        public int unit, reflector;
        public float distance, obsoleteIntensity;
    }
    [Serializable] public class NativeLight
    {
        public bool m_Enabled, m_EnableSpotReflector, m_UseColorTemperature;
        public int m_Type, m_LightUnit;
        public Color m_Color;
        public float m_Intensity, m_Range, m_SpotAngle, m_InnerSpotAngle, m_LuxAtDistance, m_ColorTemperature;
    }
    [Serializable] public class NativeWrapper { public NativeLight Light; }
    [Serializable] public class Inputs { public Input[] cases; }
    [Serializable] public class Row
    {
        public string name, before, after, companionBefore, companionAfter, error;
        public int versionBefore, versionAfter;
    }
    [Serializable] public class Dependency { public string path, sha256; }
    [Serializable] public class Report
    {
        public string unity, status, inputSha256;
        public List<Row> rows = new List<Row>();
        public List<Dependency> dependencies = new List<Dependency>();
    }
    const BindingFlags Flags = BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance | BindingFlags.Static;
    const string HdrpHash = "ff37e967ea27e617bcd6a1987c2d9d1ad2ea29533c054323e2b12574a2d6bd63";
    static string Hash(string path)
    {
        using (var sha = SHA256.Create())
        using (var input = File.OpenRead(path))
            return BitConverter.ToString(sha.ComputeHash(input)).Replace("-", "").ToLowerInvariant();
    }
    public static void Run()
    {
        string managed = Path.GetFullPath(Environment.GetEnvironmentVariable("FOA_LIGHT_MANAGED"));
        string input = Path.GetFullPath(Environment.GetEnvironmentVariable("FOA_LIGHT_INPUT"));
        string output = Path.GetFullPath(Environment.GetEnvironmentVariable("FOA_LIGHT_OUTPUT"));
        if (File.Exists(output) || output.StartsWith(managed + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase))
            throw new InvalidOperationException("An unused private output path is required.");
        var report = new Report { unity = Application.unityVersion, status = "PASSED", inputSha256 = Hash(input) };
        var observed = new Dictionary<string, string>();
        ResolveEventHandler resolver = (sender, args) =>
        {
            string name = new AssemblyName(args.Name).Name;
            var loaded = AppDomain.CurrentDomain.GetAssemblies().FirstOrDefault(a => a.GetName().Name == name);
            if (loaded != null) return loaded;
            if (name != Path.GetFileName(name)) throw new InvalidOperationException("Invalid assembly identity.");
            string path = Path.Combine(managed, name + ".dll");
            if (!File.Exists(path)) return null;
            observed[path] = Hash(path);
            return Assembly.LoadFrom(path);
        };
        GameObject owner = null;
        try
        {
            if (Application.unityVersion != "6000.0.64f1") throw new InvalidOperationException("Unqualified Unity host.");
            string assemblyPath = Path.Combine(managed, "Unity.RenderPipelines.HighDefinition.Runtime.dll");
            if (Hash(assemblyPath) != HdrpHash) throw new InvalidOperationException("Unqualified HDRP assembly.");
            observed[assemblyPath] = HdrpHash;
            var cases = JsonUtility.FromJson<Inputs>(File.ReadAllText(input)).cases;
            if (cases == null || cases.Length == 0 || cases.Length > 4096) throw new InvalidOperationException("Invalid fixture count.");
            AppDomain.CurrentDomain.AssemblyResolve += resolver;
            var assembly = Assembly.LoadFrom(assemblyPath);
            var type = assembly.GetType("UnityEngine.Rendering.HighDefinition.HDAdditionalLightData", true);
            owner = new GameObject("FoaLightMigrationFixture"); owner.SetActive(false);
            var light = owner.AddComponent<Light>(); light.type = LightType.Point;
            var companion = owner.AddComponent(type);
            var version = type.GetField("m_Version", Flags);
            var unit = type.GetField("m_LightUnit", Flags);
            var migration = type.GetFields(Flags).Single(f => f.FieldType.Name.StartsWith("MigrationDescription")).GetValue(null);
            foreach (var fixture in cases)
            {
                var row = new Row { name = fixture.name }; report.rows.Add(row);
                try
                {
                    if (fixture.reflector != 0 && fixture.reflector != 1) throw new InvalidOperationException("Invalid reflector.");
                    // Native JSON without its serializedVersion triggers Unity's own older
                    // migration. Assign the already-decoded fixture through explicit APIs.
                    var native = JsonUtility.FromJson<NativeWrapper>(fixture.nativeJson).Light;
                    light.enabled = native.m_Enabled; light.type = (LightType)native.m_Type;
                    light.color = native.m_Color; light.range = native.m_Range;
                    light.spotAngle = native.m_SpotAngle; light.innerSpotAngle = native.m_InnerSpotAngle;
                    light.lightUnit = (UnityEngine.Rendering.LightUnit)native.m_LightUnit;
                    light.luxAtDistance = native.m_LuxAtDistance;
                    light.enableSpotReflector = native.m_EnableSpotReflector;
                    light.colorTemperature = native.m_ColorTemperature;
                    light.useColorTemperature = native.m_UseColorTemperature;
                    light.intensity = native.m_Intensity;
                    if (light.type != LightType.Point) throw new InvalidOperationException("Unqualified migration shape.");
                    version.SetValue(companion, Enum.ToObject(version.FieldType, 12));
                    unit.SetValue(companion, Enum.ToObject(unit.FieldType, fixture.unit));
                    type.GetField("m_EnableSpotReflector", Flags).SetValue(companion, fixture.reflector == 1);
                    type.GetField("m_LuxAtDistance", Flags).SetValue(companion, fixture.distance);
                    type.GetField("m_Intensity", Flags).SetValue(companion, fixture.obsoleteIntensity);
                    row.before = EditorJsonUtility.ToJson(light);
                    row.companionBefore = EditorJsonUtility.ToJson(companion);
                    row.versionBefore = Convert.ToInt32(version.GetValue(companion));
                    migration.GetType().GetMethod("Migrate", Flags).Invoke(migration, new object[] { companion });
                    row.after = EditorJsonUtility.ToJson(light);
                    row.companionAfter = EditorJsonUtility.ToJson(companion);
                    row.versionAfter = Convert.ToInt32(version.GetValue(companion));
                    if (row.versionBefore != 12 || row.versionAfter != 13) throw new InvalidOperationException("Migration did not advance exactly once.");
                }
                catch (Exception ex) { row.error = ex.ToString(); report.status = "FAILED"; }
            }
            if (Hash(input) != report.inputSha256) throw new InvalidOperationException("Fixture changed during execution.");
            foreach (var entry in observed)
            {
                if (Hash(entry.Key) != entry.Value) throw new InvalidOperationException("Source assembly changed during execution.");
                report.dependencies.Add(new Dependency { path = entry.Key, sha256 = entry.Value });
            }
        }
        catch (Exception ex) { report.status = "FAILED"; report.rows.Add(new Row { name = "failure", error = ex.ToString() }); }
        finally
        {
            if (owner != null) UnityEngine.Object.DestroyImmediate(owner);
            AppDomain.CurrentDomain.AssemblyResolve -= resolver;
        }
        using (var stream = new FileStream(output, FileMode.CreateNew, FileAccess.Write))
        using (var writer = new StreamWriter(stream)) writer.Write(JsonUtility.ToJson(report, true));
        EditorApplication.Exit(report.status == "PASSED" ? 0 : 1);
    }
}
