// Copyright (c) Contributors to the Open 3D Engine Project.
// For complete copyright and license terms please see the LICENSE at the root of this distribution.
// SPDX-License-Identifier: Apache-2.0 OR MIT
// Synthetic shader defaults only. No game assets, scripts or installation writes.
using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using UnityEditor;
using UnityEngine;
using UnityEngine.Rendering;

public static class FoaTextureDefaultProbe
{
    [DllImport("FoaTextureBindingProbe")] static extern IntPtr GetCaptureEvent();
    [DllImport("FoaTextureBindingProbe")] static extern int GetFirstEvent();
    [DllImport("FoaTextureBindingProbe")] static extern int FetchCapture(int eventId, [Out] byte[] output, int capacity);
    [DllImport("FoaTextureBindingProbe")] static extern void CaptureNow(int eventId);
    static bool nativeCapture;
    static bool directCapture;
    static int captureIndex;

    [Serializable] public class Row
    {
        public string name, dimension, operation, textureType, textureName, filter, wrapU, wrapV, wrapW;
        public int width, height, anisotropy;
        public bool materialTextureIsNull;
        public Color[] pixels;
        public string nativeBinding;
    }
    [Serializable] public class Report
    {
        public string status, unityVersion, api, device, colorSpace, threadingMode, captureMode, anisotropicFiltering;
        public int qualityLevel;
        public List<Row> rows = new List<Row>();
    }
    public static void ConfigureGamma()
    {
        PlayerSettings.colorSpace = ColorSpace.Gamma;
        AssetDatabase.SaveAssets();
        EditorApplication.Exit(PlayerSettings.colorSpace == ColorSpace.Gamma ? 0 : 1);
    }
    public static void ConfigureLinear()
    {
        PlayerSettings.colorSpace = ColorSpace.Linear;
        AssetDatabase.SaveAssets();
        EditorApplication.Exit(PlayerSettings.colorSpace == ColorSpace.Linear ? 0 : 1);
    }
    public static void Run()
    {
        string output = Environment.GetEnvironmentVariable("FOA_TEXTURE_DEFAULT_OUTPUT");
        try
        {
            if (String.IsNullOrEmpty(output) || !Path.IsPathRooted(output) || !Directory.Exists(output))
                throw new InvalidOperationException("An existing private output directory is required.");
            if (Application.unityVersion != "6000.0.64f1" || SystemInfo.graphicsDeviceType != GraphicsDeviceType.Direct3D11)
                throw new InvalidOperationException("Exact Unity version and Direct3D11 are required.");
            for (var parent = new DirectoryInfo(Path.GetFullPath(output)); parent != null; parent = parent.Parent)
                if (Directory.Exists(Path.Combine(parent.FullName,".git")) || File.Exists(Path.Combine(parent.FullName,".git")))
                    throw new InvalidOperationException("Probe output must remain outside a source checkout.");
            if (File.Exists(Path.Combine(output,"texture-defaults.json"))) throw new InvalidOperationException("Probe output already exists.");
            string filtering = Environment.GetEnvironmentVariable("FOA_TEXTURE_DEFAULT_ANISOTROPY");
            if (!String.IsNullOrEmpty(filtering))
            {
                if (filtering == "Disable") QualitySettings.anisotropicFiltering = AnisotropicFiltering.Disable;
                else if (filtering == "Enable") QualitySettings.anisotropicFiltering = AnisotropicFiltering.Enable;
                else if (filtering == "ForceEnable") QualitySettings.anisotropicFiltering = AnisotropicFiltering.ForceEnable;
                else throw new InvalidOperationException("Unknown anisotropic filtering profile.");
            }
            nativeCapture = Environment.GetEnvironmentVariable("FOA_TEXTURE_DEFAULT_NATIVE") == "1";
            directCapture = nativeCapture && Environment.GetEnvironmentVariable("FOA_TEXTURE_DEFAULT_CAPTURE") == "direct";
            if (directCapture && SystemInfo.renderingThreadingMode != RenderingThreadingMode.Direct)
                throw new InvalidOperationException("Direct native capture requires verified direct rendering: " + SystemInfo.renderingThreadingMode);
            captureIndex = 0;
            if (nativeCapture && (GetCaptureEvent() == IntPtr.Zero || GetFirstEvent() < 0))
                throw new InvalidOperationException("Native D3D11 capture plugin was not initialized.");
            ShaderUtil.allowAsyncCompilation = false;
            Directory.CreateDirectory("Assets/TextureDefaultProbe");
            var report = new Report { unityVersion = Application.unityVersion, api = SystemInfo.graphicsDeviceType.ToString(),
                device = SystemInfo.graphicsDeviceName, colorSpace = QualitySettings.activeColorSpace.ToString(),
                threadingMode = SystemInfo.renderingThreadingMode.ToString(), captureMode = nativeCapture ? (directCapture ? "direct" : "event") : "none",
                anisotropicFiltering = QualitySettings.anisotropicFiltering.ToString(), qualityLevel = QualitySettings.GetQualityLevel() };
            var names = new[] { "white", "bump", "black", "linearGrey", "grey", "" };
            for (int index = 0; index < names.Length; ++index)
            {
                bool array = index == 5;
                string dimension = array ? "2DArray" : "2D";
                string shaderPath = "Assets/TextureDefaultProbe/default" + index + ".shader";
                string source = "Shader \"FOA Synthetic/Default" + index + "\" { Properties { _Probe (\"Probe\", " + dimension + ") = \"" + names[index] + "\" {} } SubShader { Pass { ZTest Always ZWrite Off Cull Off\n" +
                    "HLSLPROGRAM\n#pragma only_renderers d3d11\n#pragma target 5.0\n#pragma vertex Vert\n#pragma fragment Frag\n" +
                    "Texture" + dimension + "<float4> _Probe : register(t0); SamplerState sampler_Probe : register(s0);\n" +
                    "float4 Vert(uint id:SV_VertexID):SV_Position { return float4((id==1)?3:-1,(id==2)?3:-1,0,1); }\n" +
                    "float4 Frag(float4 p:SV_Position):SV_Target { return _Probe.SampleLevel(sampler_Probe," +
                    (array ? "float3(.25,.25,(uint)p.x%2)" : "float2(.25,.25)") + ",0); }\nENDHLSL\n} } }";
                File.WriteAllText(shaderPath, source);
                AssetDatabase.ImportAsset(shaderPath, ImportAssetOptions.ForceSynchronousImport);
                var shader = AssetDatabase.LoadAssetAtPath<Shader>(shaderPath);
                if (!shader || ShaderUtil.ShaderHasError(shader)) throw new InvalidOperationException("Default shader import failed: " + shaderPath);
                var material = new Material(shader);
                Texture overrideTexture;
                if (array)
                {
                    var texture = new Texture2DArray(1, 1, 2, TextureFormat.RGBAFloat, false, true);
                    texture.SetPixels(new[] { new Color(1,.25f,0,1) }, 0);
                    texture.SetPixels(new[] { new Color(0,.25f,1,1) }, 1);
                    texture.Apply(false, false); overrideTexture = texture;
                }
                else
                {
                    var texture = new Texture2D(1, 1, TextureFormat.RGBAFloat, false, true);
                    texture.SetPixels(new[] { new Color(1,.25f,0,1) });
                    texture.Apply(false, false); overrideTexture = texture;
                }
                if (nativeCapture)
                {
                    overrideTexture.filterMode = FilterMode.Point; overrideTexture.wrapModeU = TextureWrapMode.Clamp;
                    overrideTexture.wrapModeV = TextureWrapMode.Repeat; overrideTexture.wrapModeW = TextureWrapMode.Mirror;
                    overrideTexture.anisoLevel = 1; overrideTexture.mipMapBias = .25f;
                }
                Shader.SetGlobalTexture("_Probe", null);
                Capture(report, material, names[index], dimension, "unset");
                Shader.SetGlobalTexture("_Probe", overrideTexture);
                Capture(report, material, names[index], dimension, "global-override");
                material.SetTexture("_Probe", overrideTexture);
                Capture(report, material, names[index], dimension, "material-override");
                material.SetTexture("_Probe", null);
                Capture(report, material, names[index], dimension, "material-null-with-global");
                Shader.SetGlobalTexture("_Probe", null);
                Capture(report, material, names[index], dimension, "material-null");
                UnityEngine.Object.DestroyImmediate(material);
                UnityEngine.Object.DestroyImmediate(overrideTexture);
            }
            report.status = "PASSED";
            File.WriteAllText(Path.Combine(output, "texture-defaults.json"), JsonUtility.ToJson(report, true));
            EditorApplication.Exit(0);
        }
        catch (Exception error)
        {
            Debug.LogException(error);
            try
            {
                if (!String.IsNullOrEmpty(output) && Directory.Exists(output)) File.WriteAllText(Path.Combine(output, "failure.txt"), error.ToString());
            }
            catch (Exception writeError) { Debug.LogException(writeError); }
            finally { EditorApplication.Exit(1); }
        }
    }
    static void Capture(Report report, Material material, string name, string dimension, string operation)
    {
        var texture = material.GetTexture("_Probe");
        var row = new Row { name = name, dimension = dimension, operation = operation, materialTextureIsNull = !texture };
        if (texture)
        {
            row.textureType = texture.GetType().Name; row.textureName = texture.name;
            row.width = texture.width; row.height = texture.height; row.anisotropy = texture.anisoLevel;
            row.filter = texture.filterMode.ToString(); row.wrapU = texture.wrapModeU.ToString();
            row.wrapV = texture.wrapModeV.ToString(); row.wrapW = texture.wrapModeW.ToString();
        }
        var target = new RenderTexture(4, 1, 0, RenderTextureFormat.ARGBFloat, RenderTextureReadWrite.Linear);
        var command = new CommandBuffer(); var previous = RenderTexture.active;
        var readback = new Texture2D(4, 1, TextureFormat.RGBAFloat, false, true);
        try
        {
            if (!target.Create()) throw new InvalidOperationException("Default probe target creation failed.");
            int eventId = -1;
            if (nativeCapture)
            {
                if (captureIndex >= 64) throw new InvalidOperationException("Native capture event budget exceeded.");
                eventId = GetFirstEvent() + captureIndex++;
            }
            if (directCapture)
            {
                Graphics.SetRenderTarget(target); GL.Clear(false, true, Color.magenta);
                if (!material.SetPass(0)) throw new InvalidOperationException("Direct default material pass failed.");
                Graphics.DrawProceduralNow(MeshTopology.Triangles, 3);
                CaptureNow(eventId);
            }
            else
            {
                command.SetRenderTarget(target); command.ClearRenderTarget(false, true, Color.magenta);
                command.DrawProcedural(Matrix4x4.identity, material, 0, MeshTopology.Triangles, 3);
                if (nativeCapture) command.IssuePluginEvent(GetCaptureEvent(), eventId);
                Graphics.ExecuteCommandBuffer(command);
            }
            RenderTexture.active = target;
            readback.ReadPixels(new Rect(0,0,4,1), 0, 0); readback.Apply(); row.pixels = readback.GetPixels();
            foreach (var pixel in row.pixels)
            {
                for (int c = 0; c < 4; ++c) if (Single.IsNaN(pixel[c]) || Single.IsInfinity(pixel[c]))
                    throw new InvalidOperationException("Nonfinite default texture sample.");
                if (pixel == Color.magenta) throw new InvalidOperationException("Default draw left its clear color.");
            }
            if (nativeCapture)
            {
                var bytes = new byte[65536]; int length = FetchCapture(eventId, bytes, bytes.Length);
                if (length <= 0) throw new InvalidOperationException("Native binding event did not complete: " + length);
                row.nativeBinding = Encoding.UTF8.GetString(bytes,0,length);
                if (!row.nativeBinding.Contains("\"status\":\"PASSED\"")) throw new InvalidOperationException(row.nativeBinding);
            }
            report.rows.Add(row);
        }
        finally
        {
            RenderTexture.active = previous; command.Release(); target.Release();
            UnityEngine.Object.DestroyImmediate(target); UnityEngine.Object.DestroyImmediate(readback);
        }
    }
}
