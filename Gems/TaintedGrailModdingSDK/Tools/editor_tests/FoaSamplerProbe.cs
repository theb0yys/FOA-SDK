// Copyright (c) Contributors to the Open 3D Engine Project.
// For complete copyright and license terms please see the LICENSE at the root of this distribution.
// SPDX-License-Identifier: Apache-2.0 OR MIT
// Copy into Assets/Editor in an empty, disposable Unity project. No game inputs.
// Run with -batchmode -force-d3d11 -executeMethod FoaSamplerProbe.Run.
using System;
using System.Collections.Generic;
using System.IO;
using UnityEditor;
using UnityEngine;
using UnityEngine.Rendering;

public static class FoaSamplerProbe
{
    [Serializable] public class Case
    {
        public string name, sampler, filter, u, v, w;
        public int anisotropy;
        public bool comparison;
    }
    [Serializable] public class Report
    {
        public string status, unityVersion, graphicsApi, device;
        public bool uvStartsAtTop;
        public List<Case> cases = new List<Case>();
        public Color[] samples;
    }
    public static void Run()
    {
        try
        {
            if (SystemInfo.graphicsDeviceType != GraphicsDeviceType.Direct3D11)
                throw new InvalidOperationException("This fixture requires Direct3D 11.");
            PlayerSettings.SetUseDefaultGraphicsAPIs(BuildTarget.StandaloneWindows64, false);
            PlayerSettings.SetGraphicsAPIs(BuildTarget.StandaloneWindows64,
                new[] { GraphicsDeviceType.Direct3D11 });
            ShaderUtil.allowAsyncCompilation = false;
            var result = new Report { unityVersion = Application.unityVersion,
                graphicsApi = SystemInfo.graphicsDeviceType.ToString(),
                device = SystemInfo.graphicsDeviceName, uvStartsAtTop = SystemInfo.graphicsUVStartsAtTop };
            foreach (var filter in new[] { "Point", "Linear", "Trilinear" })
                foreach (var aniso in new[] { 0, 2, 4, 8, 16 })
                    Add(result, filter, "Repeat", "Repeat", "Repeat", aniso, false);
            foreach (var wrap in new[] { "Clamp", "Mirror", "MirrorOnce" })
            {
                Add(result, "Linear", wrap, "Repeat", "Repeat", 0, false);
                Add(result, "Linear", "Repeat", wrap, "Repeat", 0, false);
                Add(result, "Linear", "Repeat", "Repeat", wrap, 0, false);
            }
            Add(result, "Point", "Clamp", "Clamp", "Clamp", 0, true);
            Add(result, "Linear", "Clamp", "Clamp", "Clamp", 0, true);
            Add(result, "Linear", "Clamp", "Mirror", "MirrorOnce", 4, false);
            Directory.CreateDirectory("Assets/Probe");
            var paths = new List<string>();
            foreach (var item in result.cases)
            {
                string path = "Assets/Probe/" + item.name + ".shader";
                string sample = item.comparison
                    ? "_Probe.SampleCmpLevelZero(" + item.sampler + ", uv, 0.5).xxxx"
                    : "_Probe.SampleLevel(" + item.sampler + ", uv, 0)";
                string source = "Shader \"FOA Synthetic/" + item.name + "\" { SubShader { Pass { ZTest Always ZWrite Off Cull Off\n" +
                    "HLSLPROGRAM\n#pragma only_renderers d3d11\n#pragma target 5.0\n#pragma vertex Vert\n#pragma fragment Frag\n" +
                    "Texture2D<float4> _Probe; " + (item.comparison ? "SamplerComparisonState " : "SamplerState ") +
                    item.sampler + "; float4 _Coordinates[4];\n" +
                    "float4 Vert(uint id:SV_VertexID):SV_Position { return float4((id==1)?3:-1,(id==2)?3:-1,0,1); }\n" +
                    "float4 Frag(float4 position:SV_Position):SV_Target { float2 uv=_Coordinates[(uint)position.x].xy; return " +
                    sample + "; }\nENDHLSL\n} } }";
                File.WriteAllText(path, source);
                paths.Add(path);
            }
            AssetDatabase.Refresh(ImportAssetOptions.ForceSynchronousImport);
            foreach (var path in paths)
            {
                var shader = AssetDatabase.LoadAssetAtPath<Shader>(path);
                if (!shader || ShaderUtil.ShaderHasError(shader))
                    throw new InvalidOperationException("Synthetic shader import failed: " + path);
            }
            Directory.CreateDirectory("ProbeOutput");
            var build = new AssetBundleBuild { assetBundleName = "sampler-probe", assetNames = paths.ToArray() };
            if (!BuildPipeline.BuildAssetBundles("ProbeOutput", new[] { build },
                    BuildAssetBundleOptions.ForceRebuildAssetBundle | BuildAssetBundleOptions.ChunkBasedCompression,
                    BuildTarget.StandaloneWindows64))
                throw new InvalidOperationException("Synthetic shader bundle build failed.");
            // Row-zero is defined by these supplied raw bytes, independent of any image decoder.
            var texture = new Texture2D(2, 2, TextureFormat.RGBA32, false, true);
            texture.LoadRawTextureData(new byte[] {
                255,0,0,255, 0,255,0,255, 0,0,255,255, 255,255,0,255 });
            texture.Apply(false, false);
            AssetDatabase.CreateAsset(texture, "Assets/Probe/orientation.asset");
            AssetDatabase.SaveAssets();
            // Native GPU readback is four samples of explicitly supplied UVs. No Blit UV convention.
            var material = new Material(AssetDatabase.LoadAssetAtPath<Shader>(paths[0]));
            material.SetTexture("_Probe", texture);
            material.SetVectorArray("_Coordinates", new[] {
                new Vector4(.25f,.25f,0,0), new Vector4(.75f,.25f,0,0),
                new Vector4(.25f,.75f,0,0), new Vector4(.75f,.75f,0,0) });
            var target = new RenderTexture(4, 1, 0, RenderTextureFormat.ARGBFloat, RenderTextureReadWrite.Linear);
            target.Create();
            var command = new CommandBuffer();
            command.SetRenderTarget(target);
            command.ClearRenderTarget(false, true, Color.magenta);
            command.DrawProcedural(Matrix4x4.identity, material, 0, MeshTopology.Triangles, 3);
            Graphics.ExecuteCommandBuffer(command);
            RenderTexture.active = target;
            var readback = new Texture2D(4, 1, TextureFormat.RGBAFloat, false, true);
            readback.ReadPixels(new Rect(0,0,4,1), 0, 0);
            readback.Apply();
            result.samples = readback.GetPixels();
            var expected = new[] { Color.red, Color.green, Color.blue, new Color(1,1,0,1) };
            for (int i=0;i<4;i++)
                if (Vector4.Distance(result.samples[i], expected[i]) > .00001f)
                    throw new InvalidOperationException("GPU raw-row UV check failed at sample " + i);
            RenderTexture.active = null;
            command.Release();
            target.Release();
            UnityEngine.Object.DestroyImmediate(target);
            UnityEngine.Object.DestroyImmediate(material);
            UnityEngine.Object.DestroyImmediate(readback);
            result.status = "PASSED";
            File.WriteAllText("ProbeOutput/result.json", JsonUtility.ToJson(result, true));
            EditorApplication.Exit(0);
        }
        catch (Exception error)
        {
            Directory.CreateDirectory("ProbeOutput");
            File.WriteAllText("ProbeOutput/failure.txt", error.ToString());
            Debug.LogException(error);
            EditorApplication.Exit(1);
        }
    }
    static void Add(Report report, string filter, string u, string v, string w, int anisotropy, bool comparison)
    {
        string sampler = "sampler_" + filter + "_" + u + "U_" + v + "V_" + w + "W" +
            (anisotropy == 0 ? "" : "_Aniso" + anisotropy) + (comparison ? "_Compare" : "");
        report.cases.Add(new Case { name = "case_" + report.cases.Count.ToString("D2"),
            sampler = sampler, filter = filter, u = u, v = v, w = w,
            anisotropy = anisotropy, comparison = comparison });
    }
}
