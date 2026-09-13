// Copyright (c) Contributors to the Open 3D Engine Project.
// For complete copyright and license terms please see the LICENSE at the root of this distribution.
// SPDX-License-Identifier: Apache-2.0 OR MIT
// Isolated original-host measurement. This fixture supplies explicit synthetic material inputs.
using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices;
using UnityEditor;
using UnityEngine;
using UnityEngine.Rendering;

public static class FoaMaterialConstantProbe
{
    [DllImport("FoaMaterialBindingProbe")] static extern int ReadConstants(int stage, int slot, int bytes,
        [Out] byte[] output, int capacity, [Out] uint[] metadata);
    [Serializable] public class BufferRow { public int stage, slot; public uint[] metadata; public string hex; }
    [Serializable] public class CaseRow
    {
        public string name, pixelsHex, linearHex, defaultHex;
        public List<BufferRow> buffers = new List<BufferRow>();
        public List<string> rejections = new List<string>();
    }
    [Serializable] public class Report
    {
        public string status, unityVersion, api, threading, colorSpace;
        public List<CaseRow> rows = new List<CaseRow>();
    }
    static string Hex(byte[] value) { return BitConverter.ToString(value).Replace("-", "").ToLowerInvariant(); }
    static void PrivatePath(string path)
    {
        if (String.IsNullOrEmpty(path) || !Path.IsPathRooted(path)) throw new InvalidOperationException("Explicit private path required.");
        for (var item = new DirectoryInfo(Path.GetFullPath(path)); item != null; item = item.Parent)
            if (File.Exists(Path.Combine(item.FullName,".git")) || Directory.Exists(Path.Combine(item.FullName,".git")))
                throw new InvalidOperationException("Fixture and outputs must stay outside Git.");
    }
    public static void ConfigureGamma() { PlayerSettings.colorSpace = ColorSpace.Gamma; AssetDatabase.SaveAssets(); EditorApplication.Exit(0); }
    public static void ConfigureLinear() { PlayerSettings.colorSpace = ColorSpace.Linear; AssetDatabase.SaveAssets(); EditorApplication.Exit(0); }
    public static void Run()
    {
        string output = Environment.GetEnvironmentVariable("FOA_MATERIAL_CONSTANT_OUTPUT");
        bool outputAccepted = false;
        try
        {
            PrivatePath(output); PrivatePath(Application.dataPath);
            if (!Directory.Exists(output) || File.Exists(Path.Combine(output,"constants.json"))) throw new InvalidOperationException("Fresh output required.");
            outputAccepted = true;
            if (Application.unityVersion != "6000.0.64f1" || SystemInfo.graphicsDeviceType != GraphicsDeviceType.Direct3D11 ||
                SystemInfo.renderingThreadingMode != RenderingThreadingMode.Direct) throw new InvalidOperationException("Qualified direct Unity D3D11 required.");
            ShaderUtil.allowAsyncCompilation = false;
            Directory.CreateDirectory("Assets/MaterialConstantProbe");
            const string shaderSource = @"Shader ""FOA/ConstantProbe"" {
Properties {
_Color (""Color"", Color) = (.25,.5,2,.375)
_Vector (""Vector"", Vector) = (.25,.5,2,.375)
[Gamma] _GammaVector (""GammaVector"", Vector) = (.25,.5,2,.375)
[HDR] _HdrColor (""HdrColor"", Color) = (.25,.5,2,.375)
_Float (""Float"", Float) = .5
[Gamma] _GammaFloat (""GammaFloat"", Float) = .5
_Range (""Range"", Range(0,1)) = .5
[Gamma] _GammaRange (""GammaRange"", Range(0,1)) = .5
_Texture (""Texture"", 2D) = ""white"" {}
}
SubShader { Pass { ZTest Always ZWrite Off Cull Off
HLSLPROGRAM
#pragma target 5.0
#pragma vertex VS
#pragma fragment PS
cbuffer UnityPerMaterial : register(b4) {
 float4 _Color; float4 _Vector; float4 _GammaVector; float4 _HdrColor;
 float _Float; float _GammaFloat; float _Range; float _GammaRange; float4 _Texture_ST;
};
float4 VS(uint id : SV_VertexID) : SV_Position {
 float2 p = float2((id << 1) & 2, id & 2);
 return float4(p * 2 - 1 + float2(0,_Float * 0.000001), .5, 1);
}
float4 PS(float4 p : SV_Position) : SV_Target {
 uint col = (uint)p.x;
 if (col == 0) return _Color;
 if (col == 1) return _Vector;
 if (col == 2) return _GammaVector;
 if (col == 3) return _HdrColor;
 if (col == 4) return float4(_Float,_GammaFloat,_Range,_GammaRange);
 return _Texture_ST;
}
ENDHLSL
} } }";
            File.WriteAllText("Assets/MaterialConstantProbe/probe.shader",shaderSource);
            AssetDatabase.ImportAsset("Assets/MaterialConstantProbe/probe.shader",ImportAssetOptions.ForceSynchronousImport);
            var shader = AssetDatabase.LoadAssetAtPath<Shader>("Assets/MaterialConstantProbe/probe.shader");
            if (!shader || !shader.isSupported || ShaderUtil.ShaderHasError(shader)) throw new InvalidOperationException("Synthetic shader failed.");
            var report = new Report { status="PARTIAL", unityVersion=Application.unityVersion, api=SystemInfo.graphicsDeviceType.ToString(),
                threading=SystemInfo.renderingThreadingMode.ToString(), colorSpace=QualitySettings.activeColorSpace.ToString() };
            var material = new Material(shader);
            try
            {
                Capture(report,material,"defaults");
                material.SetColor("_Color",new Color(.25f,.5f,2f,.375f));
                Capture(report,material,"set-identical-default");
                material.SetColor("_Color",new Color(.1f,.25f,4f,.7f)); material.SetColor("_HdrColor",new Color(.1f,.25f,4f,.7f));
                material.SetVector("_Vector",new Vector4(.1f,.25f,4f,.7f)); material.SetVector("_GammaVector",new Vector4(.1f,.25f,4f,.7f));
                material.SetFloat("_Float",.25f); material.SetFloat("_GammaFloat",.25f); material.SetFloat("_Range",2f); material.SetFloat("_GammaRange",2f);
                material.SetTextureScale("_Texture",new Vector2(-2f,1.5f)); material.SetTextureOffset("_Texture",new Vector2(.125f,-.25f));
                Capture(report,material,"set-properties");
                // SetVector on a Color declaration distinguishes API getter values from upload semantics.
                material.SetVector("_Color",new Vector4(.2f,.4f,.6f,.8f));
                material.SetColor("_Vector",new Color(.2f,.4f,.6f,.8f));
                Capture(report,material,"cross-setters");
                string path="Assets/MaterialConstantProbe/saved-"+QualitySettings.activeColorSpace+".mat";
                if(File.Exists(path)) throw new InvalidOperationException("Fresh saved fixture required.");
                AssetDatabase.CreateAsset(new Material(material),path); AssetDatabase.SaveAssets();
                AssetDatabase.ImportAsset(path,ImportAssetOptions.ForceSynchronousImport | ImportAssetOptions.ForceUpdate);
                Capture(report,AssetDatabase.LoadAssetAtPath<Material>(path),"saved-reloaded");
            }
            finally { UnityEngine.Object.DestroyImmediate(material); }
            report.status="PASSED";
            File.WriteAllText(Path.Combine(output,"constants.json"),JsonUtility.ToJson(report,true));
            EditorApplication.Exit(0);
        }
        catch (Exception error)
        {
            Debug.LogException(error);
            try
            {
                // A rejected output path must not become writable through error reporting.
                if (outputAccepted)
                    using (var writer = new StreamWriter(new FileStream(Path.Combine(output,"failure.txt"),FileMode.CreateNew,FileAccess.Write,FileShare.None)))
                        writer.Write(error.ToString());
            }
            catch (Exception diagnosticError) { Debug.LogException(diagnosticError); }
            finally { EditorApplication.Exit(1); }
        }
    }
    static void Capture(Report report, Material material, string name)
    {
        var row=new CaseRow {name=name}; var old=RenderTexture.active;
        var linear=material.GetColor("_Color").linear;
        var defaults=material.shader.GetPropertyDefaultVectorValue(material.shader.FindPropertyIndex("_Color"));
        var conversion=new byte[16]; var raw=new byte[16];
        for(int c=0;c<4;++c) { Buffer.BlockCopy(BitConverter.GetBytes(linear[c]),0,conversion,c*4,4); Buffer.BlockCopy(BitConverter.GetBytes(defaults[c]),0,raw,c*4,4); }
        row.linearHex=Hex(conversion); row.defaultHex=Hex(raw);
        var target=new RenderTexture(6,1,0,RenderTextureFormat.ARGBFloat,RenderTextureReadWrite.Linear);
        var pixels=new Texture2D(6,1,TextureFormat.RGBAFloat,false,true);
        try
        {
            if (!target.Create()) throw new InvalidOperationException("Target failed.");
            Graphics.SetRenderTarget(target); GL.Clear(false,true,Color.magenta);
            if (!material.SetPass(0)) throw new InvalidOperationException("Material pass failed.");
            Graphics.DrawProceduralNow(MeshTopology.Triangles,3);
            for (int stage=0;stage<2;++stage) for (int slot=0;slot<14;++slot)
            {
                var bytes=new byte[96]; var metadata=new uint[4]; int count=ReadConstants(stage,slot,96,bytes,96,metadata);
                if (count==96) row.buffers.Add(new BufferRow {stage=stage,slot=slot,metadata=metadata,hex=Hex(bytes)});
                else if (count!=-4 && count!=-5) throw new InvalidOperationException("Readback failed: "+count);
            }
            foreach (var bad in new[] {new[]{-1,0,96,96},new[]{2,0,96,96},new[]{0,-1,96,96},new[]{0,14,96,96},
                new[]{0,0,0,96},new[]{0,0,17,96},new[]{0,0,65552,96},new[]{0,0,96,95},new[]{0,0,96,65537}})
            {
                var bytes=new byte[96]; for(int i=0;i<bytes.Length;++i) bytes[i]=0x5a;
                var metadata=new uint[]{1,2,3,4}; string before=Hex(bytes);
                if(ReadConstants(bad[0],bad[1],bad[2],bytes,bad[3],metadata)!=-1 || before!=Hex(bytes) || metadata[0]!=1 || metadata[3]!=4)
                    throw new InvalidOperationException("Invalid read mutated output.");
                row.rejections.Add(String.Join(",",bad));
            }
            RenderTexture.active=target; pixels.ReadPixels(new Rect(0,0,6,1),0,0); pixels.Apply();
            row.pixelsHex=Hex(pixels.GetRawTextureData<byte>().ToArray());
            if(row.buffers.Count!=2 || row.buffers[0].slot!=4 || row.buffers[1].slot!=4 ||
                row.buffers[0].hex!=row.pixelsHex || row.buffers[1].hex!=row.pixelsHex)
                throw new InvalidOperationException("Shader outputs differ from the exact bound material bytes.");
            report.rows.Add(row);
        }
        finally { RenderTexture.active=old; target.Release(); UnityEngine.Object.DestroyImmediate(target); UnityEngine.Object.DestroyImmediate(pixels); }
    }
}
