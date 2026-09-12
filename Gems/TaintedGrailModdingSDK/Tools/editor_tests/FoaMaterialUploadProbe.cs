// Copyright (c) Contributors to the Open 3D Engine Project.
// For complete copyright and license terms please see the LICENSE at the root of this distribution.
// SPDX-License-Identifier: Apache-2.0 OR MIT
// Isolated saved-property upload measurement. This does not execute source shader passes.
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

public static class FoaMaterialUploadProbe
{
    const string Profile="unity-6000.0.64f1-d3d11";
    [DllImport("FoaMaterialBindingProbe")] static extern int ReadConstants(int stage,int slot,int bytes,[Out] byte[] output,int capacity,[Out] uint[] metadata);
    [Serializable] public class Property { public string name,kind,hex; public int flags; }
    [Serializable] public class MaterialRow { public string key,source_sha256; public Property[] properties; }
    [Serializable] public class Request { public string schema,profile,color_space; public int version; public MaterialRow[] materials; }
    [Serializable] public class Value { public string name,hex; }
    [Serializable] public class Result { public string key,source_sha256; public List<Value> properties=new List<Value>(); }
    [Serializable] public class Receipt
    {
        public string schema="foa-material-upload-receipt",profile=Profile,color_space,input_sha256,status="PARTIAL",api,threading;
        public int version=1,shader_count;
        public List<Result> materials=new List<Result>();
    }
    static void Require(bool condition,string message) { if(!condition) throw new InvalidOperationException(message); }
    static string Hex(byte[] bytes) { return BitConverter.ToString(bytes).Replace("-","").ToLowerInvariant(); }
    static string Sha(byte[] bytes) { using(var sha=SHA256.Create()) return Hex(sha.ComputeHash(bytes)); }
    static void PrivatePath(string path)
    {
        Require(!String.IsNullOrEmpty(path) && Path.IsPathRooted(path),"Explicit private path required.");
        for(var item=new DirectoryInfo(Path.GetFullPath(path));item!=null;item=item.Parent)
            Require(!Directory.Exists(Path.Combine(item.FullName,".git")) && !File.Exists(Path.Combine(item.FullName,".git")),"Upload fixtures must stay outside Git.");
    }
    static bool Vector(Property prop) { return prop.kind=="Color" || prop.kind=="Vector"; }
    static float[] Values(Property prop)
    {
        int count=Vector(prop)?4:1;
        Require(prop.hex!=null && prop.hex.Length==count*8 && Regex.IsMatch(prop.hex,"\\A[0-9a-f]+\\z"),"Exact input float bytes required.");
        var values=new float[count];
        for(int i=0;i<count;++i)
        {
            var bytes=new byte[4]; for(int b=0;b<4;++b) bytes[b]=Convert.ToByte(prop.hex.Substring(i*8+b*2,2),16);
            values[i]=BitConverter.ToSingle(bytes,0);
            Require(!Single.IsNaN(values[i]) && !Single.IsInfinity(values[i]),"Nonfinite material input is not qualified.");
        }
        return values;
    }
    static void Validate(Property prop)
    {
        Require(prop!=null && !String.IsNullOrEmpty(prop.name) && prop.name.Length<=1024 && !prop.name.Contains("\0"),"Invalid material property name.");
        bool color=prop.kind=="Color";
        Require(color || prop.kind=="Vector" || prop.kind=="Float" || prop.kind=="Range","Unqualified material property type.");
        Require(prop.flags==0 || prop.flags==1 || (color?(prop.flags==16 || prop.flags==256):prop.flags==32),"Unqualified material property flags.");
        Values(prop);
    }
    static Shader CreateShader(MaterialRow row, string folder, Dictionary<string,Shader> cache)
    {
        var signature=new StringBuilder(); foreach(var prop in row.properties) signature.Append(prop.kind).Append(':').Append(prop.flags).Append(';');
        string identity=Sha(Encoding.UTF8.GetBytes(signature.ToString()));
        if(cache.TryGetValue(identity,out var cached)) return cached;
        var shader=new StringBuilder("Shader \"FOA/MaterialUpload/").Append(identity).Append("\" { Properties {\n");
        for(int i=0;i<row.properties.Length;++i)
        {
            var prop=row.properties[i];
            string flag=prop.flags==1?"[HideInInspector]":prop.flags==16?"[HDR]":prop.flags==256?"[MainColor]":prop.flags==32?"[Gamma]":"";
            shader.Append(flag).Append(" _Foa").Append(i).Append(" (\"Value\", ")
                .Append(prop.kind=="Range"?"Range(0,1)":prop.kind).Append(") = ").Append(Vector(prop)?"(0,0,0,0)":"0").Append('\n');
        }
        shader.Append("} SubShader { Pass { ZTest Always ZWrite Off Cull Off\nHLSLPROGRAM\n#pragma only_renderers d3d11\n#pragma target 5.0\n#pragma vertex VS\n#pragma fragment PS\ncbuffer UnityPerMaterial : register(b4) {\n");
        for(int i=0;i<row.properties.Length;++i)
            shader.Append(Vector(row.properties[i])?"float4 ":"float ").Append("_Foa").Append(i).Append(" : packoffset(c").Append(i).Append(");\n");
        shader.Append("};\nfloat4 VS(uint id:SV_VertexID):SV_Position { return float4((id==1)?3:-1,(id==2)?3:-1,.5,1); }\n")
            .Append("float4 PS(float4 p:SV_Position):SV_Target { uint col=(uint)p.x;\n");
        for(int i=0;i<row.properties.Length;++i)
        {
            shader.Append("if(col==").Append(i).Append(") return ");
            shader.Append(Vector(row.properties[i])?"_Foa":"float4(_Foa").Append(i).Append(Vector(row.properties[i])?";\n":",0,0,0);\n");
        }
        shader.Append("return 0; }\nENDHLSL\n} } }");
        string path=folder+"/"+identity+".shader"; string source=shader.ToString();
        if(File.Exists(path)) Require(File.ReadAllText(path)==source,"Existing fixture shader differs.");
        else File.WriteAllText(path,source);
        AssetDatabase.ImportAsset(path,ImportAssetOptions.ForceSynchronousImport);
        var result=AssetDatabase.LoadAssetAtPath<Shader>(path);
        Require(result && result.isSupported && !ShaderUtil.ShaderHasError(result),"Material upload fixture compilation failed.");
        cache.Add(identity,result); return result;
    }
    static Result Capture(MaterialRow source, Shader shader)
    {
        int count=source.properties.Length, size=count*16;
        var material=new Material(shader);
        var target=new RenderTexture(count,1,0,RenderTextureFormat.ARGBFloat,RenderTextureReadWrite.Linear);
        var pixels=new Texture2D(count,1,TextureFormat.RGBAFloat,false,true);
        var previous=RenderTexture.active;
        try
        {
            for(int i=0;i<count;++i)
            {
                var prop=source.properties[i]; var values=Values(prop); string name="_Foa"+i;
                if(prop.kind=="Color") material.SetColor(name,new Color(values[0],values[1],values[2],values[3]));
                else if(prop.kind=="Vector") material.SetVector(name,new Vector4(values[0],values[1],values[2],values[3]));
                else material.SetFloat(name,values[0]);
            }
            Require(target.Create(),"Material upload target creation failed.");
            Graphics.SetRenderTarget(target); GL.Clear(false,true,Color.magenta);
            Require(material.SetPass(0),"Material upload pass failed."); Graphics.DrawProceduralNow(MeshTopology.Triangles,3);
            var buffer=new byte[size]; var metadata=new uint[4];
            Require(ReadConstants(1,4,size,buffer,size,metadata)==size,"Direct constant-buffer readback failed.");
            RenderTexture.active=target; pixels.ReadPixels(new Rect(0,0,count,1),0,0); pixels.Apply();
            var image=pixels.GetRawTextureData<byte>().ToArray(); var result=new Result { key=source.key,source_sha256=source.source_sha256 };
            for(int i=0;i<count;++i)
            {
                int length=Vector(source.properties[i])?16:4; var value=new byte[length];
                for(int b=0;b<length;++b)
                {
                    Require(buffer[i*16+b]==image[i*16+b],"GPU output disagrees with measured material constant.");
                    value[b]=buffer[i*16+b];
                }
                for(int b=0;b<length;b+=4) Require(!Single.IsNaN(BitConverter.ToSingle(value,b)) && !Single.IsInfinity(BitConverter.ToSingle(value,b)),"Nonfinite material upload is not qualified.");
                result.properties.Add(new Value {name=source.properties[i].name,hex=Hex(value)});
            }
            return result;
        }
        finally { RenderTexture.active=previous; target.Release(); UnityEngine.Object.DestroyImmediate(target); UnityEngine.Object.DestroyImmediate(pixels); UnityEngine.Object.DestroyImmediate(material); }
    }
    public static void Run()
    {
        string output=Environment.GetEnvironmentVariable("FOA_MATERIAL_UPLOAD_OUTPUT");
        bool outputAccepted = false;
        try
        {
            string input=Environment.GetEnvironmentVariable("FOA_MATERIAL_UPLOAD_INPUT");
            PrivatePath(input); PrivatePath(output); PrivatePath(Application.dataPath);
            Require(File.Exists(input) && new FileInfo(input).Length<=64*1024*1024 && Directory.Exists(output) && !File.Exists(Path.Combine(output,"uploads.json")),"Bounded input and fresh private output required.");
            outputAccepted = true;
            Require(Application.unityVersion=="6000.0.64f1" && SystemInfo.graphicsDeviceType==GraphicsDeviceType.Direct3D11 &&
                SystemInfo.renderingThreadingMode==RenderingThreadingMode.Direct && BitConverter.IsLittleEndian,"Qualified direct Unity D3D11 host required.");
            byte[] inputBytes=File.ReadAllBytes(input); var request=JsonUtility.FromJson<Request>(Encoding.UTF8.GetString(inputBytes));
            Require(request!=null && request.schema=="foa-material-upload-request" && request.version==1 && request.profile==Profile &&
                request.color_space==QualitySettings.activeColorSpace.ToString(),"Material upload profile/color space mismatch.");
            Require(request.materials!=null && request.materials.Length>0 && request.materials.Length<=1024,"Material batch exceeds its bound.");
            var keys=new HashSet<string>(); int total=0;
            foreach(var row in request.materials)
            {
                Require(row!=null && row.key!=null && Regex.IsMatch(row.key,"\\A[0-9a-f]{32}\\z") && keys.Add(row.key) &&
                    row.source_sha256!=null && Regex.IsMatch(row.source_sha256,"\\A[0-9a-f]{64}\\z"),"Invalid/duplicate material identity.");
                Require(row.properties!=null && row.properties.Length>0 && row.properties.Length<=4096 && row.properties.Length<=SystemInfo.maxTextureSize,"Material upload exceeds its bound.");
                total+=row.properties.Length; Require(total<=262144,"Material batch work exceeds its bound.");
                var names=new HashSet<string>(); foreach(var prop in row.properties) { Validate(prop); Require(names.Add(prop.name),"Duplicate material property."); }
            }
            ShaderUtil.allowAsyncCompilation=false;
            string folder="Assets/MaterialUploadProbe"; Directory.CreateDirectory(folder);
            var cache=new Dictionary<string,Shader>();
            var receipt=new Receipt { color_space=request.color_space,input_sha256=Sha(inputBytes),api=SystemInfo.graphicsDeviceType.ToString(),threading=SystemInfo.renderingThreadingMode.ToString() };
            foreach(var row in request.materials) receipt.materials.Add(Capture(row,CreateShader(row,folder,cache)));
            receipt.shader_count=cache.Count; receipt.status="PASSED";
            File.WriteAllText(Path.Combine(output,"uploads.json"),JsonUtility.ToJson(receipt,true)); EditorApplication.Exit(0);
        }
        catch(Exception error)
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
}
