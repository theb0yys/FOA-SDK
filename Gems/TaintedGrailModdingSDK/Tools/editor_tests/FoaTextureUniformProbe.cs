// Copyright (c) Contributors to the Open 3D Engine Project.
// For complete copyright and license terms please see the LICENSE at the root of this distribution.
// SPDX-License-Identifier: Apache-2.0 OR MIT
// Isolated Texture2D uniform measurement. Original shader passes and live game overrides are not exercised.
using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text;
using System.Text.RegularExpressions;
using UnityEditor;
using UnityEngine;
using UnityEngine.Rendering;

public static class FoaTextureUniformProbe
{
    const string Profile = "unity-6000.0.64f1-d3d11";
    [DllImport("FoaMaterialBindingProbe", CallingConvention=CallingConvention.Cdecl)]
    static extern int ReadConstants(int stage, int slot, int bytes, [Out] byte[] output, int capacity, [Out] uint[] metadata);
    [Serializable] public class Resource
    {
        public string key, kind, bundle_sha256, record_sha256, asset_path;
        public int width, height, mip_count, format;
    }
    [Serializable] public class Request
    {
        public string schema, profile, color_space;
        public int version, mip_limit;
        public bool streaming;
        public Resource[] resources;
    }
    [Serializable] public class Measurement
    {
        public string key, operation, hex, dimensions_hex;
        public int width, height, mip_count, format;
    }
    [Serializable] public class Receipt
    {
        public string schema="foa-texture-uniform-receipt", profile=Profile, status, input_sha256, color_space, api, threading;
        public int version=1, mip_limit;
        public bool streaming;
        public List<Measurement> resources=new List<Measurement>();
        public List<Measurement> controls=new List<Measurement>();
    }
    static void Require(bool value,string message) { if(!value) throw new InvalidOperationException(message); }
    static string Hex(byte[] bytes) { return BitConverter.ToString(bytes).Replace("-","").ToLowerInvariant(); }
    static string Sha(byte[] bytes) { using(var sha=SHA256.Create()) return Hex(sha.ComputeHash(bytes)); }
    static bool IsHash(string value) { return value!=null && Regex.IsMatch(value,"\\A[0-9a-f]{64}\\z"); }
    static void PrivatePath(string path)
    {
        Require(!String.IsNullOrEmpty(path) && Path.IsPathRooted(path),"Explicit private path required.");
        for(var item=new DirectoryInfo(Path.GetFullPath(path));item!=null;item=item.Parent)
        {
            Require(!Directory.Exists(Path.Combine(item.FullName,".git")) && !File.Exists(Path.Combine(item.FullName,".git")),"Texture fixtures must stay outside Git.");
            if(item.Exists) Require((item.Attributes & FileAttributes.ReparsePoint)==0,"Private fixture links are not accepted.");
        }
        if(File.Exists(path)) Require((File.GetAttributes(path) & FileAttributes.ReparsePoint)==0,"Private fixture file links are not accepted.");
    }
    static Shader CreateShader()
    {
        const string source="Shader \"FOA/TextureUniform\" { Properties { _HeightMap (\"HeightMap\", 2D) = \"black\" {} } SubShader { Pass { ZTest Always ZWrite Off Cull Off\n"+
            "HLSLPROGRAM\n#pragma only_renderers d3d11\n#pragma target 5.0\n#pragma vertex VS\n#pragma fragment PS\n"+
            "Texture2D<float4> _HeightMap; cbuffer UnityPerMaterial : register(b4) { float4 _HeightMap_TexelSize; };\n"+
            "float4 VS(uint id:SV_VertexID):SV_Position { return float4((id==1)?3:-1,(id==2)?3:-1,.5,1); }\n"+
            "float4 PS(float4 p:SV_Position):SV_Target { uint w,h,m; _HeightMap.GetDimensions(0,w,h,m); if(p.x<1) return _HeightMap_TexelSize; if(p.x<2) return float4(w,h,m,1); return _HeightMap.Load(int3(0,0,0)); }\nENDHLSL\n} } }";
        const string folder="Assets/TextureUniformProbe", path=folder+"/probe.shader";
        Directory.CreateDirectory(folder);
        if(File.Exists(path)) Require(File.ReadAllText(path)==source,"Existing texture fixture differs.");
        else File.WriteAllText(path,source);
        AssetDatabase.ImportAsset(path,ImportAssetOptions.ForceSynchronousImport);
        var shader=AssetDatabase.LoadAssetAtPath<Shader>(path);
        Require(shader && shader.isSupported && !ShaderUtil.ShaderHasError(shader),"Texture uniform shader compilation failed.");
        return shader;
    }
    static Measurement Capture(Material material, Texture2D texture, string key, string operation)
    {
        var target=new RenderTexture(3,1,0,RenderTextureFormat.ARGBFloat,RenderTextureReadWrite.Linear);
        var image=new Texture2D(3,1,TextureFormat.RGBAFloat,false,true);
        var previous=RenderTexture.active;
        try
        {
            Require(target.Create(),"Texture uniform output creation failed.");
            Graphics.SetRenderTarget(target); GL.Clear(false,true,Color.magenta);
            Require(material.SetPass(0),"Texture uniform pass failed."); Graphics.DrawProceduralNow(MeshTopology.Triangles,3);
            var buffer=new byte[16]; var metadata=new uint[4];
            Require(ReadConstants(1,4,16,buffer,16,metadata)==16,"Texture uniform constant readback failed.");
            RenderTexture.active=target; image.ReadPixels(new Rect(0,0,3,1),0,0); image.Apply();
            var pixels=image.GetRawTextureData<byte>().ToArray(); var dimensions=new byte[16];
            for(int i=0;i<16;++i) { Require(buffer[i]==pixels[i],"Texture uniform GPU output differs from its constant buffer."); dimensions[i]=pixels[i+16]; }
            for(int i=0;i<48;i+=4) Require(!Single.IsNaN(BitConverter.ToSingle(pixels,i)) && !Single.IsInfinity(BitConverter.ToSingle(pixels,i)),"Nonfinite texture measurement.");
            if(texture)
                Require(BitConverter.ToSingle(dimensions,0)==texture.width && BitConverter.ToSingle(dimensions,4)==texture.height &&
                    BitConverter.ToSingle(dimensions,8)==texture.mipmapCount && BitConverter.ToSingle(dimensions,12)==1,"Bound resource differs from full-resolution Texture2D.");
            return new Measurement {key=key,operation=operation,hex=Hex(buffer),dimensions_hex=Hex(dimensions),
                width=texture?texture.width:0,height=texture?texture.height:0,mip_count=texture?texture.mipmapCount:0,format=texture?(int)texture.format:0};
        }
        finally { RenderTexture.active=previous; target.Release(); UnityEngine.Object.DestroyImmediate(target); UnityEngine.Object.DestroyImmediate(image); }
    }
    static void Controls(Receipt receipt, Shader shader)
    {
        var material=new Material(shader); var control=new Texture2D(7,3,TextureFormat.RGBA32,false,true);
        try
        {
            var pixels=new Color[21]; for(int i=0;i<pixels.Length;++i) pixels[i]=new Color(i/32f,.25f,.5f,1);
            control.SetPixels(pixels); control.Apply(false,false);
            Shader.SetGlobalTexture("_HeightMap",null);
            receipt.controls.Add(Capture(material,null,"control","unset"));
            Shader.SetGlobalTexture("_HeightMap",control);
            receipt.controls.Add(Capture(material,null,"control","global-with-unset"));
            material.SetTexture("_HeightMap",null);
            receipt.controls.Add(Capture(material,null,"control","global-with-material-null"));
            material.SetTexture("_HeightMap",control);
            receipt.controls.Add(Capture(material,control,"control","non-square"));
            material.SetTextureScale("_HeightMap",new Vector2(-2,3)); material.SetTextureOffset("_HeightMap",new Vector2(.25f,-.5f));
            receipt.controls.Add(Capture(material,control,"control","non-square-st"));
            material.SetTexture("_HeightMap",null); Shader.SetGlobalTexture("_HeightMap",null);
            receipt.controls.Add(Capture(material,null,"control","null-after-control"));
        }
        finally { Shader.SetGlobalTexture("_HeightMap",null); UnityEngine.Object.DestroyImmediate(material); UnityEngine.Object.DestroyImmediate(control); }
    }
    public static void ConfigureGamma()
    {
        PrivatePath(Application.dataPath); PlayerSettings.colorSpace=ColorSpace.Gamma;
        AssetDatabase.SaveAssets(); EditorApplication.Exit(PlayerSettings.colorSpace==ColorSpace.Gamma?0:1);
    }
    public static void ConfigureLinear()
    {
        PrivatePath(Application.dataPath); PlayerSettings.colorSpace=ColorSpace.Linear;
        AssetDatabase.SaveAssets(); EditorApplication.Exit(PlayerSettings.colorSpace==ColorSpace.Linear?0:1);
    }
    public static void Run()
    {
        string output=Environment.GetEnvironmentVariable("FOA_TEXTURE_UNIFORM_OUTPUT"); bool accepted=false;
        int oldLimit=QualitySettings.globalTextureMipmapLimit; bool oldStreaming=QualitySettings.streamingMipmapsActive;
        try
        {
            string input=Environment.GetEnvironmentVariable("FOA_TEXTURE_UNIFORM_INPUT"), bundles=Environment.GetEnvironmentVariable("FOA_TEXTURE_UNIFORM_BUNDLES");
            PrivatePath(input); PrivatePath(output); PrivatePath(bundles); PrivatePath(Application.dataPath);
            Require(File.Exists(input) && new FileInfo(input).Length<=4*1024*1024 && Directory.Exists(output) &&
                !File.Exists(Path.Combine(output,"uniforms.json")) && !File.Exists(Path.Combine(output,"failure.txt")),"Bounded input and fresh private output required.");
            accepted=true;
            Require(Application.unityVersion=="6000.0.64f1" && SystemInfo.graphicsDeviceType==GraphicsDeviceType.Direct3D11 &&
                SystemInfo.renderingThreadingMode==RenderingThreadingMode.Direct && BitConverter.IsLittleEndian,"Exact direct Unity D3D11 host required.");
            var bytes=File.ReadAllBytes(input); var request=JsonUtility.FromJson<Request>(Encoding.UTF8.GetString(bytes));
            Require(request!=null && request.schema=="foa-texture-uniform-request" && request.version==1 && request.profile==Profile &&
                request.color_space==QualitySettings.activeColorSpace.ToString() && request.mip_limit==0 && !request.streaming,"Unsupported texture upload profile.");
            Require(request.resources!=null && request.resources.Length>0 && request.resources.Length<=64,"Texture measurement batch exceeds its bound.");
            var keys=new HashSet<string>();
            foreach(var row in request.resources)
            {
                Require(row!=null && IsHash(row.key) && keys.Add(row.key),"Invalid or duplicate texture identity.");
                Require(row.kind=="black-default" || row.kind=="asset","Unsupported texture binding kind.");
                if(row.kind=="asset")
                    Require(IsHash(row.bundle_sha256) && IsHash(row.record_sha256) && !String.IsNullOrEmpty(row.asset_path) && row.asset_path.Length<=1024 &&
                        row.asset_path.IndexOf('\0')<0 && row.width>0 && row.width<=8192 && row.height>0 && row.height<=8192 &&
                        row.mip_count>0 && row.mip_count<=14 && (row.format==3 || row.format==26),"Unqualified source Texture2D descriptor.");
                else Require(row.width==0 && row.height==0 && row.mip_count==0 && row.format==0 &&
                    String.IsNullOrEmpty(row.bundle_sha256) && String.IsNullOrEmpty(row.record_sha256) && String.IsNullOrEmpty(row.asset_path),"Default texture has unexpected asset fields.");
            }
            QualitySettings.globalTextureMipmapLimit=0; QualitySettings.streamingMipmapsActive=false; ShaderUtil.allowAsyncCompilation=false;
            var shader=CreateShader(); var receipt=new Receipt {input_sha256=Sha(bytes),color_space=request.color_space,
                api=SystemInfo.graphicsDeviceType.ToString(),threading=SystemInfo.renderingThreadingMode.ToString(),mip_limit=0,streaming=false};
            Controls(receipt,shader);
            foreach(var row in request.resources)
            {
                AssetBundle bundle=null; var material=new Material(shader);
                try
                {
                    Texture2D texture=null;
                    if(row.kind=="asset")
                    {
                        string path=Path.Combine(bundles,row.bundle_sha256+".bundle"); PrivatePath(path);
                        Require(File.Exists(path) && new FileInfo(path).Length<=128*1024*1024,"Missing or oversized private texture bundle.");
                        Require(Sha(File.ReadAllBytes(path))==row.bundle_sha256,"Private texture bundle hash mismatch.");
                        bundle=AssetBundle.LoadFromFile(path); Require(bundle,"Source texture bundle failed to load.");
                        texture=bundle.LoadAsset<Texture2D>(row.asset_path);
                        Require(texture && texture.width==row.width && texture.height==row.height && texture.mipmapCount==row.mip_count &&
                            (int)texture.format==row.format,"Loaded source texture metadata mismatch.");
                    }
                    material.SetTexture("_HeightMap",texture);
                    receipt.resources.Add(Capture(material,texture,row.key,"material"));
                }
                finally { UnityEngine.Object.DestroyImmediate(material); if(bundle) bundle.Unload(true); }
            }
            receipt.status="PASSED";
            using(var writer=new StreamWriter(new FileStream(Path.Combine(output,"uniforms.json"),FileMode.CreateNew,FileAccess.Write,FileShare.None)))
                writer.Write(JsonUtility.ToJson(receipt,true));
            QualitySettings.globalTextureMipmapLimit=oldLimit; QualitySettings.streamingMipmapsActive=oldStreaming; EditorApplication.Exit(0);
        }
        catch(Exception error)
        {
            Debug.LogException(error);
            try
            {
                if(accepted) using(var writer=new StreamWriter(new FileStream(Path.Combine(output,"failure.txt"),FileMode.CreateNew,FileAccess.Write,FileShare.None))) writer.Write(error.ToString());
            }
            catch(Exception diagnosticError) { Debug.LogException(diagnosticError); }
            finally { QualitySettings.globalTextureMipmapLimit=oldLimit; QualitySettings.streamingMipmapsActive=oldStreaming; EditorApplication.Exit(1); }
        }
    }
}
