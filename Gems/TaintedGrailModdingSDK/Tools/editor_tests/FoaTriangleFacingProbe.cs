// Copyright (c) Contributors to the Open 3D Engine Project.
// For complete copyright and license terms please see the LICENSE at the root of this distribution.
// SPDX-License-Identifier: Apache-2.0 OR MIT
// Measure procedural clip-space triangle facing; camera/pass inversion remains separately owned.
using System;
using System.Collections.Generic;
using System.IO;
using UnityEditor;
using UnityEngine;
using UnityEngine.Rendering;

public static class FoaTriangleFacingProbe
{
    [Serializable] public class Case { public bool reversed, invert_culling, front_face; }
    [Serializable] public class Report
    {
        public string status, unity_version, api, threading, context="direct-procedural-rendertexture";
        public List<Case> cases=new List<Case>();
    }
    static void Require(bool value,string error) { if(!value) throw new InvalidOperationException(error); }
    static void Private(string path)
    {
        Require(Path.IsPathRooted(path),"Private absolute path required.");
        for(var p=new DirectoryInfo(Path.GetFullPath(path));p!=null;p=p.Parent)
            Require(!Directory.Exists(Path.Combine(p.FullName,".git")) && !File.Exists(Path.Combine(p.FullName,".git")) &&
                (!p.Exists || (p.Attributes&FileAttributes.ReparsePoint)==0),"Private fixture cannot be in Git or traverse links.");
    }
    public static void Run()
    {
        string output=Environment.GetEnvironmentVariable("FOA_TRIANGLE_FACING_OUTPUT"); bool accepted=false,success=false,old=GL.invertCulling;
        Material material=null; RenderTexture target=null; Texture2D image=null; var previous=RenderTexture.active;
        try
        {
            Require(!String.IsNullOrEmpty(output),"Fresh private output required."); Private(output); Private(Application.dataPath);
            Require(Directory.Exists(output) && !File.Exists(Path.Combine(output,"facing.json")) && !File.Exists(Path.Combine(output,"failure.txt")),"Fresh private output required."); accepted=true;
            Require(Application.unityVersion=="6000.0.64f1" && SystemInfo.graphicsDeviceType==GraphicsDeviceType.Direct3D11 &&
                SystemInfo.renderingThreadingMode==RenderingThreadingMode.Direct,"Exact direct D3D11 host required.");
            ShaderUtil.allowAsyncCompilation=false; Directory.CreateDirectory("Assets/TriangleFacingProbe");
            const string path="Assets/TriangleFacingProbe/probe.shader";
            const string source="Shader \"FOA/Facing\" { Properties { _Reverse (\"Reverse\",Float)=0 } SubShader { Pass { Cull Off ZTest Always ZWrite Off\nHLSLPROGRAM\n#pragma only_renderers d3d11\n#pragma target 5.0\n#pragma vertex VS\n#pragma fragment PS\nfloat _Reverse; float4 VS(uint id:SV_VertexID):SV_Position { if(_Reverse>0 && id>0) id=3-id; return float4(id==0?-.75:.75,id==2?.5:-.5,.5,1); } float4 PS(float4 p:SV_Position,bool face:SV_IsFrontFace):SV_Target { return float4(face?1:0,.25,.5,1); }\nENDHLSL\n} } }";
            if(File.Exists(path)) Require(File.ReadAllText(path)==source,"Existing facing shader differs."); else File.WriteAllText(path,source);
            AssetDatabase.ImportAsset(path,ImportAssetOptions.ForceSynchronousImport);var shader=AssetDatabase.LoadAssetAtPath<Shader>(path);
            Require(shader && !ShaderUtil.ShaderHasError(shader),"Facing shader failed.");material=new Material(shader);
            target=new RenderTexture(64,64,0,RenderTextureFormat.ARGBFloat,RenderTextureReadWrite.Linear);Require(target.Create(),"Facing target failed.");
            image=new Texture2D(64,64,TextureFormat.RGBAFloat,false,true);var report=new Report {unity_version=Application.unityVersion,api=SystemInfo.graphicsDeviceType.ToString(),threading=SystemInfo.renderingThreadingMode.ToString()};
            foreach(bool invert in new[]{false,true}) foreach(bool reverse in new[]{false,true})
            {
                GL.invertCulling=invert;material.SetFloat("_Reverse",reverse?1:0);Graphics.SetRenderTarget(target);GL.Clear(false,true,Color.magenta);
                Require(material.SetPass(0),"Facing pass failed.");Graphics.DrawProceduralNow(MeshTopology.Triangles,3);RenderTexture.active=target;
                image.ReadPixels(new Rect(0,0,64,64),0,0);image.Apply();Color pixel=image.GetPixel(48,24);
                Require((pixel.r==0 || pixel.r==1) && pixel.g==.25f && pixel.b==.5f && pixel.a==1,"Facing triangle did not cover the measured pixel.");
                report.cases.Add(new Case {reversed=reverse,invert_culling=invert,front_face=pixel.r==1});
            }
            Require(report.cases[0].front_face!=report.cases[1].front_face && report.cases[0].front_face!=report.cases[2].front_face && report.cases[0].front_face==report.cases[3].front_face,"Facing/invert controls did not differ.");
            report.status="PASSED";using(var writer=new StreamWriter(new FileStream(Path.Combine(output,"facing.json"),FileMode.CreateNew,FileAccess.Write,FileShare.None))) writer.Write(JsonUtility.ToJson(report,true));
            success=true;
        }
        catch(Exception error)
        {
            Debug.LogException(error);
            try { if(accepted) using(var writer=new StreamWriter(new FileStream(Path.Combine(output,"failure.txt"),FileMode.CreateNew,FileAccess.Write,FileShare.None))) writer.Write(error.ToString()); }
            catch(Exception diagnosticError) { Debug.LogException(diagnosticError); }
        }
        finally
        {
            GL.invertCulling=old;RenderTexture.active=previous;if(target) { target.Release();UnityEngine.Object.DestroyImmediate(target); }
            if(image) UnityEngine.Object.DestroyImmediate(image);if(material) UnityEngine.Object.DestroyImmediate(material);
            EditorApplication.Exit(success?0:1);
        }
    }
}
