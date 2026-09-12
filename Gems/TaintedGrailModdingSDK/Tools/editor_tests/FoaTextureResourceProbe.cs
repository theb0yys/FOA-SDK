// Copyright (c) Contributors to the Open 3D Engine Project.
// For complete copyright and license terms please see the LICENSE at the root of this distribution.
// SPDX-License-Identifier: Apache-2.0 OR MIT
// Synthetic defaults only. GPU GetDimensions/Load results do not establish a sampler descriptor or runtime globals.
using System;
using System.Collections.Generic;
using System.IO;
using UnityEditor;
using UnityEngine;
using UnityEngine.Rendering;

public static class FoaTextureResourceProbe
{
    [Serializable] public class Mip
    {
        public int level, layer, width, height;
        public Color[] pixels;
    }
    [Serializable] public class Resource
    {
        public string name, dimension, operation;
        public int width, height, layers, levels;
        public List<Mip> mips = new List<Mip>();
    }
    [Serializable] public class Report
    {
        public string status, unityVersion, api, device, colorSpace;
        public List<Resource> resources = new List<Resource>();
    }
    static string Pass(string declaration, string fragment)
    {
        return "Pass { ZTest Always ZWrite Off Cull Off\nHLSLPROGRAM\n#pragma only_renderers d3d11\n#pragma target 5.0\n#pragma vertex Vert\n#pragma fragment Frag\n" +
            declaration + "\nuint _Mip; uint _Layer;\nfloat4 Vert(uint id:SV_VertexID):SV_Position { return float4((id==1)?3:-1,(id==2)?3:-1,0,1); }\n" +
            "float4 Frag(float4 p:SV_Position):SV_Target { " + fragment + " }\nENDHLSL\n}";
    }
    static Color[] Draw(Material material, int pass, int width, int height)
    {
        var target = new RenderTexture(width, height, 0, RenderTextureFormat.ARGBFloat, RenderTextureReadWrite.Linear);
        var readback = new Texture2D(width, height, TextureFormat.RGBAFloat, false, true);
        var command = new CommandBuffer(); var previous = RenderTexture.active;
        try
        {
            if (!target.Create()) throw new InvalidOperationException("Resource probe target creation failed.");
            command.SetRenderTarget(target); command.ClearRenderTarget(false, true, Color.magenta);
            command.DrawProcedural(Matrix4x4.identity, material, pass, MeshTopology.Triangles, 3);
            Graphics.ExecuteCommandBuffer(command); RenderTexture.active = target;
            readback.ReadPixels(new Rect(0,0,width,height), 0, 0); readback.Apply();
            var pixels = readback.GetPixels();
            foreach (var pixel in pixels) for (int c = 0; c < 4; ++c)
                if (Single.IsNaN(pixel[c]) || Single.IsInfinity(pixel[c])) throw new InvalidOperationException("Nonfinite resource sample.");
            return pixels;
        }
        finally
        {
            RenderTexture.active = previous; command.Release(); target.Release();
            UnityEngine.Object.DestroyImmediate(target); UnityEngine.Object.DestroyImmediate(readback);
        }
    }
    static int Extent(float value, int maximum)
    {
        int integer = Mathf.RoundToInt(value);
        if (value != integer || integer < 1 || integer > maximum) throw new InvalidOperationException("Unqualified resource extent: " + value);
        return integer;
    }
    static void Capture(Report report, Material material, string name, bool array, string operation)
    {
        material.SetInt("_Mip", 0); material.SetInt("_Layer", 0);
        Color dimensions = Draw(material, 0, 1, 1)[0];
        var row = new Resource { name = name, dimension = array ? "2DArray" : "2D", operation = operation,
            width = Extent(dimensions.r, 64), height = Extent(dimensions.g, 64),
            layers = Extent(dimensions.b, 16), levels = Extent(dimensions.a, 16) };
        if (!array && row.layers != 1) throw new InvalidOperationException("A 2D resource reported array layers.");
        int texels = 0;
        for (int mip = 0; mip < row.levels; ++mip)
        {
            material.SetInt("_Mip", mip); dimensions = Draw(material, 0, 1, 1)[0];
            int width = Extent(dimensions.r, 64), height = Extent(dimensions.g, 64);
            if (width != Math.Max(1,row.width >> mip) || height != Math.Max(1,row.height >> mip) ||
                dimensions.b != row.layers || dimensions.a != row.levels) throw new InvalidOperationException("Inconsistent mip resource extents.");
            for (int layer = 0; layer < row.layers; ++layer)
            {
                texels += width * height;
                if (texels > 65536) throw new InvalidOperationException("Resource sampling budget exceeded.");
                material.SetInt("_Layer", layer);
                row.mips.Add(new Mip { level = mip, layer = layer, width = width, height = height,
                    pixels = Draw(material, 1, width, height) });
            }
        }
        report.resources.Add(row);
    }
    static Texture Control(bool array)
    {
        Texture2D two = array ? null : new Texture2D(4, 4, TextureFormat.RGBAFloat, true, true);
        Texture2DArray layers = array ? new Texture2DArray(4, 4, 2, TextureFormat.RGBAFloat, true, true) : null;
        for (int layer = 0; layer < (array ? 2 : 1); ++layer) for (int mip = 0; mip < 3; ++mip)
        {
            int size = 4 >> mip; var pixels = new Color[size*size];
            for (int y = 0; y < size; ++y) for (int x = 0; x < size; ++x)
                pixels[y*size+x] = new Color((x+1)/8.0f+mip/16.0f,(y+1)/8.0f,layer/4.0f+mip/8.0f,1.0f);
            if (array) layers.SetPixels(pixels, layer, mip); else two.SetPixels(pixels, mip);
        }
        if (array) { layers.Apply(false, false); return layers; }
        two.Apply(false, false); return two;
    }
    public static void ConfigureGamma()
    {
        PlayerSettings.colorSpace = ColorSpace.Gamma; AssetDatabase.SaveAssets();
        EditorApplication.Exit(PlayerSettings.colorSpace == ColorSpace.Gamma ? 0 : 1);
    }
    public static void Run()
    {
        string output = Environment.GetEnvironmentVariable("FOA_TEXTURE_RESOURCE_OUTPUT");
        try
        {
            if (String.IsNullOrEmpty(output) || !Path.IsPathRooted(output) || !Directory.Exists(output) ||
                File.Exists(Path.Combine(output,"texture-resources.json"))) throw new InvalidOperationException("A new private output is required.");
            for (var path = new DirectoryInfo(output); path != null; path = path.Parent)
                if (Directory.Exists(Path.Combine(path.FullName,".git")) || File.Exists(Path.Combine(path.FullName,".git")))
                    throw new InvalidOperationException("Keep fixture output outside source checkouts.");
            if (Application.unityVersion != "6000.0.64f1" || SystemInfo.graphicsDeviceType != GraphicsDeviceType.Direct3D11)
                throw new InvalidOperationException("Exact Unity and Direct3D11 are required.");
            ShaderUtil.allowAsyncCompilation = false; Directory.CreateDirectory("Assets/TextureResourceProbe");
            var report = new Report { unityVersion = Application.unityVersion, api = SystemInfo.graphicsDeviceType.ToString(),
                device = SystemInfo.graphicsDeviceName, colorSpace = QualitySettings.activeColorSpace.ToString() };
            var names = new[] { "white", "bump", "black", "linearGrey", "grey", "" };
            for (int index = 0; index < names.Length; ++index)
            {
                bool array = index == 5; string dimension = array ? "2DArray" : "2D";
                string declaration = "Texture" + dimension + "<float4> _Probe;";
                string dimensions = "uint w,h,n=1,m; _Probe.GetDimensions(_Mip,w,h," + (array ? "n," : "") + "m); return float4(w,h,n,m);";
                string load = "return _Probe.Load(" + (array ? "int4((int2)p.xy,_Layer,_Mip)" : "int3((int2)p.xy,_Mip)") + ");";
                string shaderPath = "Assets/TextureResourceProbe/resource" + index + ".shader";
                File.WriteAllText(shaderPath,"Shader \"FOA Synthetic/Resource" + index + "\" { Properties { _Probe (\"Probe\", " + dimension + ") = \"" + names[index] + "\" {} } SubShader { " + Pass(declaration,dimensions) + Pass(declaration,load) + " } }");
                AssetDatabase.ImportAsset(shaderPath,ImportAssetOptions.ForceSynchronousImport);
                var shader = AssetDatabase.LoadAssetAtPath<Shader>(shaderPath);
                if (!shader || ShaderUtil.ShaderHasError(shader)) throw new InvalidOperationException("Resource shader failed: " + shaderPath);
                var material = new Material(shader); Texture control = null;
                try
                {
                    Shader.SetGlobalTexture("_Probe",null);
                    Capture(report,material,names[index],array,"unset");
                    control = Control(array); material.SetTexture("_Probe",control);
                    Capture(report,material,names[index],array,"material-control");
                    material.SetTexture("_Probe",null);
                    Capture(report,material,names[index],array,"material-null");
                }
                finally
                {
                    Shader.SetGlobalTexture("_Probe",null); UnityEngine.Object.DestroyImmediate(material);
                    if (control) UnityEngine.Object.DestroyImmediate(control);
                }
            }
            report.status = "PASSED";
            File.WriteAllText(Path.Combine(output,"texture-resources.json"),JsonUtility.ToJson(report,true)); EditorApplication.Exit(0);
        }
        catch (Exception error)
        {
            if (!String.IsNullOrEmpty(output) && Directory.Exists(output)) File.WriteAllText(Path.Combine(output,"failure.txt"),error.ToString());
            Debug.LogException(error); EditorApplication.Exit(1);
        }
    }
}
