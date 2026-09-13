// Copyright (c) Contributors to the Open 3D Engine Project.
// For complete copyright and license terms please see the LICENSE at the root of this distribution.
// SPDX-License-Identifier: Apache-2.0 OR MIT
// Synthetic Alpha8 GPU fixture. Copy into a disposable Unity 6000.0.64f1 project.
// Run -batchmode -force-d3d11 -executeMethod FoaAlphaProbe.Run; no game inputs.
using System;
using System.Collections.Generic;
using System.IO;
using UnityEditor;
using UnityEngine;
using UnityEngine.Rendering;
public static class FoaAlphaProbe
{
 [Serializable] public class Row { public bool linear; public string graphicsFormat; public Color[] samples; public byte[] payload; public int width, height, mipCount; }
 [Serializable] public class Report { public string status, unityVersion, graphicsApi, device; public int alpha8Enum; public List<Row> rows=new List<Row>(); }
 public static void Run()
 {
  try
  {
   if(Application.unityVersion!="6000.0.64f1" || SystemInfo.graphicsDeviceType!=GraphicsDeviceType.Direct3D11) throw new Exception("Unexpected Unity GPU profile");
   if(!SystemInfo.SupportsTextureFormat(TextureFormat.Alpha8)) throw new Exception("Alpha8 unsupported on fixture GPU");
   ShaderUtil.allowAsyncCompilation=false;
   Directory.CreateDirectory("Assets/Probe");Directory.CreateDirectory("ProbeOutput");
   File.WriteAllText("Assets/Probe/alpha.shader", "Shader \"FOA Synthetic/Alpha8\" { SubShader { Pass { ZTest Always ZWrite Off Cull Off\nHLSLPROGRAM\n#pragma only_renderers d3d11\n#pragma target 5.0\n#pragma vertex Vert\n#pragma fragment Frag\nTexture2D<float4> _Probe; SamplerState sampler_Point_Clamp; float4 _Coordinates[21];\nfloat4 Vert(uint id:SV_VertexID):SV_Position { return float4((id==1)?3:-1,(id==2)?3:-1,0,1); }\nfloat4 Frag(float4 position:SV_Position):SV_Target { float3 c=_Coordinates[(uint)position.x].xyz; return _Probe.SampleLevel(sampler_Point_Clamp,c.xy,c.z); }\nENDHLSL\n} } }");
   AssetDatabase.Refresh(ImportAssetOptions.ForceSynchronousImport);
   var shader=AssetDatabase.LoadAssetAtPath<Shader>("Assets/Probe/alpha.shader");
   if(!shader || ShaderUtil.ShaderHasError(shader)) throw new Exception("Alpha shader failed");
   var report=new Report { unityVersion=Application.unityVersion, graphicsApi=SystemInfo.graphicsDeviceType.ToString(),device=SystemInfo.graphicsDeviceName,alpha8Enum=(int)TextureFormat.Alpha8 };
   byte[] raw={0,17,34,51,68,85,102,119,136,153,170,187,204,221,238,255,43,87,131,175,219};
   var coordinates=new List<Vector4>();
   for(int mip=0,w=4;mip<3;mip++,w/=2) for(int y=0;y<w;y++) for(int x=0;x<w;x++) coordinates.Add(new Vector4((x+.5f)/w,(y+.5f)/w,mip,0));
   foreach(bool linear in new[]{true,false})
   {
    var texture=new Texture2D(4,4,TextureFormat.Alpha8,3,linear);texture.LoadRawTextureData(raw);texture.Apply(false,false);
    var material=new Material(shader);material.SetTexture("_Probe",texture);material.SetVectorArray("_Coordinates",coordinates);
    var target=new RenderTexture(21,1,0,RenderTextureFormat.ARGBFloat,RenderTextureReadWrite.Linear);target.Create();
    var command=new CommandBuffer();command.SetRenderTarget(target);command.ClearRenderTarget(false,true,Color.magenta);command.DrawProcedural(Matrix4x4.identity,material,0,MeshTopology.Triangles,3);Graphics.ExecuteCommandBuffer(command);
    RenderTexture.active=target;
    var readback=new Texture2D(21,1,TextureFormat.RGBAFloat,false,true);readback.ReadPixels(new Rect(0,0,21,1),0,0);readback.Apply();
    var colors=readback.GetPixels();
    for(int i=0;i<raw.Length;i++) if(colors[i].r!=0 || colors[i].g!=0 || colors[i].b!=0 || Mathf.Abs(colors[i].a-raw[i]/255f)>.000001f) throw new Exception("Source alpha/mip sample mismatch at "+i);
    report.rows.Add(new Row{linear=linear,graphicsFormat=texture.graphicsFormat.ToString(),samples=colors,payload=raw,width=4,height=4,mipCount=texture.mipmapCount});
    RenderTexture.active=null;command.Release();target.Release();UnityEngine.Object.DestroyImmediate(target);UnityEngine.Object.DestroyImmediate(material);UnityEngine.Object.DestroyImmediate(readback);UnityEngine.Object.DestroyImmediate(texture);
   }
   report.status="PASSED";File.WriteAllText("ProbeOutput/result.json",JsonUtility.ToJson(report,true));EditorApplication.Exit(0);
  }
  catch(Exception error) { Directory.CreateDirectory("ProbeOutput");File.WriteAllText("ProbeOutput/failure.txt",error.ToString());Debug.LogException(error);EditorApplication.Exit(1); }
 }
}
