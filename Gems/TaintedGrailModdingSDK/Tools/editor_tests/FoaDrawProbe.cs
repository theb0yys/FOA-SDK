// Copyright (c) Contributors to the Open 3D Engine Project.
// For complete copyright and license terms please see the LICENSE at the root of this distribution.
// SPDX-License-Identifier: Apache-2.0 OR MIT
// Synthetic Unity 6000.0.64f1 D3D11 BatchRendererGroup submesh/draw-multiplicity fixture.
// Copy to a disposable project's Assets/Editor with Assets/csc.rsp containing -unsafe.
// Run -batchmode -force-d3d11 -executeMethod FoaDrawProbe.Run; no game inputs.
// The source C# material/submesh ordinal chain is independently inspected evidence.
using System;
using System.Collections.Generic;
using System.IO;
using Unity.Collections;
using Unity.Collections.LowLevel.Unsafe;
using Unity.Jobs;
using UnityEditor;
using UnityEngine;
using UnityEngine.Rendering;
public class FoaDrawPipelineAsset : RenderPipelineAsset<FoaDrawPipeline>
{
 public override string renderPipelineShaderTag => "FOASynthetic";
 protected override RenderPipeline CreatePipeline() { return new FoaDrawPipeline(); }
}
public class FoaDrawPipeline : RenderPipeline
{
 protected override void Render(ScriptableRenderContext context, Camera[] cameras)
 {
  foreach(var camera in cameras)
  {
   if(camera!=FoaDrawProbe.Camera)continue;
   try
   {
    if(!camera.TryGetCullingParameters(out var parameters))throw new Exception("No camera cull parameters");
    var cull=context.Cull(ref parameters);
    context.SetupCameraProperties(camera);
    var cb=new CommandBuffer();cb.SetRenderTarget(camera.targetTexture);cb.ClearRenderTarget(true,true,Color.black);context.ExecuteCommandBuffer(cb);cb.Release();
    var drawing=new DrawingSettings(new ShaderTagId("SRPDefaultUnlit"),new SortingSettings(camera)){enableInstancing=true};
    var filtering=new FilteringSettings(RenderQueueRange.all);
    context.DrawRenderers(cull,ref drawing,ref filtering);context.Submit();
    FoaDrawProbe.Frames++;
   }
   catch(Exception e){FoaDrawProbe.Fail(e);}
  }
 }
}
public static class FoaDrawProbe
{
 [Serializable] public class Row {public string name;public int submeshes;public int[] draws;public int callbacks;public Color[] samples;public string capture;public List<string> messages;}
 [Serializable] public class Report {public string status,unityVersion,api,device;public List<Row> rows=new List<Row>();}
 public static Camera Camera;public static int Frames;
 static BatchRendererGroup brg;static GraphicsBuffer buffer;static BatchID batch;static BatchMeshID[] meshes;static BatchMaterialID[] materials;
 static RenderTexture target;static FoaDrawPipelineAsset pipeline;static int index,callbacks;static double started;static bool failed;
 static int[][] rules={new[]{0},new[]{1},new[]{2},new[]{0,1,2},new[]{0},new[]{0,1,2},new[]{0,1,2},new[]{3},new[]{0,1,2}};
 static int[] counts={3,3,3,3,2,1,2,3,1};static string[] names={"first_only","second_only","third_only","all_three","two_submeshes_one_material","one_submesh_three_materials","two_submeshes_three_materials","overflow_on_three","additive_one_submesh_three_materials"};
 static List<string> messages=new List<string>();static Report report;
 public static void Run()
 {
  try
  {
   if(Application.unityVersion!="6000.0.64f1" || SystemInfo.graphicsDeviceType!=GraphicsDeviceType.Direct3D11)throw new Exception("Wrong Unity GPU profile");
   Directory.CreateDirectory("ProbeOutput");Directory.CreateDirectory("Assets/Probe");ShaderUtil.allowAsyncCompilation=false;
   File.WriteAllText("Assets/Probe/draw.shader","Shader \"FOA Synthetic/DrawOrdinal\" { Properties { _ProbeColor(\"Color\",Color)=(1,1,1,1) } SubShader { Tags { \"RenderType\"=\"Opaque\" } Pass { Tags { \"LightMode\"=\"SRPDefaultUnlit\" } Cull Off ZWrite Off ZTest Always\nHLSLPROGRAM\n#pragma target 4.5\n#pragma only_renderers d3d11\n#pragma vertex V\n#pragma fragment F\n#pragma multi_compile _ DOTS_INSTANCING_ON\ncbuffer UnityPerMaterial {float4 _ProbeColor;};\nfloat4 V(float3 position:POSITION):SV_POSITION{return float4(position,1);}\nfloat4 F():SV_Target{return _ProbeColor;}\nENDHLSL\n} } }");
   AssetDatabase.Refresh(ImportAssetOptions.ForceSynchronousImport);
   var shader=AssetDatabase.LoadAssetAtPath<Shader>("Assets/Probe/draw.shader");if(!shader || ShaderUtil.ShaderHasError(shader))throw new Exception("Draw shader compilation failed");
   report=new Report{status="PARTIAL",unityVersion=Application.unityVersion,api=SystemInfo.graphicsDeviceType.ToString(),device=SystemInfo.graphicsDeviceName};
   brg=new BatchRendererGroup(Cull,IntPtr.Zero);brg.SetGlobalBounds(new Bounds(Vector3.zero,Vector3.one*1000));
   buffer=new GraphicsBuffer(GraphicsBuffer.Target.Raw,16,4);buffer.SetData(new uint[16]);
   using(var metadata=new NativeArray<MetadataValue>(0,Allocator.Temp)){batch=brg.AddBatch(metadata,buffer.bufferHandle);}
   meshes=new BatchMeshID[3];materials=new BatchMaterialID[6];
   for(int n=1;n<=3;n++)
   {
    var mesh=new Mesh();var vertices=new List<Vector3>();
    for(int q=0;q<n;q++){float x=-.66f+q*.66f;vertices.Add(new Vector3(x-.22f,-.3f,.5f));vertices.Add(new Vector3(x+.22f,-.3f,.5f));vertices.Add(new Vector3(x+.22f,.3f,.5f));vertices.Add(new Vector3(x-.22f,.3f,.5f));}
    mesh.SetVertices(vertices);mesh.subMeshCount=n;for(int q=0;q<n;q++)mesh.SetTriangles(new[]{4*q,4*q+1,4*q+2,4*q,4*q+2,4*q+3},q);mesh.RecalculateBounds();meshes[n-1]=brg.RegisterMesh(mesh);
    var material=new Material(shader){enableInstancing=true};material.SetColor("_ProbeColor",new[]{Color.red,Color.green,Color.blue}[n-1]);materials[n-1]=brg.RegisterMaterial(material);    string additiveSource=File.ReadAllText("Assets/Probe/draw.shader").Replace("FOA Synthetic/DrawOrdinal", "FOA Synthetic/DrawOrdinalAdditive").Replace("Cull Off ZWrite Off", "Blend One One Cull Off ZWrite Off");
    if(n==1){File.WriteAllText("Assets/Probe/additive.shader",additiveSource);AssetDatabase.Refresh(ImportAssetOptions.ForceSynchronousImport);}
    var additiveShader=AssetDatabase.LoadAssetAtPath<Shader>("Assets/Probe/additive.shader");if(!additiveShader || ShaderUtil.ShaderHasError(additiveShader))throw new Exception("Additive shader failed");
    var additiveMaterial=new Material(additiveShader){enableInstancing=true};additiveMaterial.SetColor("_ProbeColor",new[]{Color.red,Color.green,Color.blue}[n-1]*.125f);materials[n+2]=brg.RegisterMaterial(additiveMaterial);
   }
   target=new RenderTexture(384,128,24,RenderTextureFormat.ARGB32,RenderTextureReadWrite.Linear);target.Create();
   Camera=new GameObject("DrawOrdinalCamera").AddComponent<Camera>();Camera.transform.position=new Vector3(0,0,-10);Camera.orthographic=true;Camera.orthographicSize=3;Camera.nearClipPlane=.1f;Camera.farClipPlane=100;Camera.targetTexture=target;Camera.enabled=true;
   GraphicsSettings.useScriptableRenderPipelineBatching=true;pipeline=ScriptableObject.CreateInstance<FoaDrawPipelineAsset>();GraphicsSettings.defaultRenderPipeline=pipeline;QualitySettings.renderPipeline=pipeline;
   Application.logMessageReceived+=Message;started=EditorApplication.timeSinceStartup;EditorApplication.update+=Tick;
  }
  catch(Exception e){Fail(e);}
 }
 static void Message(string text,string stack,LogType kind){if(kind==LogType.Error || kind==LogType.Exception || kind==LogType.Warning)messages.Add(kind+": "+text);}
 static unsafe JobHandle Cull(BatchRendererGroup group,BatchCullingContext context,BatchCullingOutput output,IntPtr user)
 {
  callbacks++;int n=rules[index].Length;var d=(BatchCullingOutputDrawCommands*)output.drawCommands.GetUnsafePtr();*d=default;
  d->drawCommands=(BatchDrawCommand*)UnsafeUtility.Malloc(sizeof(BatchDrawCommand)*n,16,Allocator.TempJob);
  d->drawRanges=(BatchDrawRange*)UnsafeUtility.Malloc(sizeof(BatchDrawRange),16,Allocator.TempJob);
  d->visibleInstances=(int*)UnsafeUtility.Malloc(sizeof(int)*2,16,Allocator.TempJob);d->visibleInstances[0]=0;d->visibleInstances[1]=1;
  d->drawCommandCount=n;d->drawRangeCount=1;d->visibleInstanceCount=2;
  for(int i=0;i<n;i++)d->drawCommands[i]=new BatchDrawCommand{batchID=batch,meshID=meshes[counts[index]-1],materialID=materials[i+(index==8?3:0)],submeshIndex=(ushort)rules[index][i],visibleOffset=0,visibleCount=2,splitVisibilityMask=0xff};
  d->drawRanges[0]=new BatchDrawRange{drawCommandsType=BatchDrawCommandType.Direct,drawCommandsBegin=0,drawCommandsCount=(uint)n,filterSettings=new BatchFilterSettings{renderingLayerMask=uint.MaxValue,layer=0,shadowCastingMode=ShadowCastingMode.Off,receiveShadows=false}};
  return default;
 }
 static void Tick()
 {
  try
  {
   if(failed)return;if(EditorApplication.timeSinceStartup-started>100)throw new Exception("Timed out waiting for native BRG frames");
   if(Frames>=3)
   {
    RenderTexture.active=target;var image=new Texture2D(target.width,target.height,TextureFormat.RGBA32,false,true);image.ReadPixels(new Rect(0,0,target.width,target.height),0,0);image.Apply();RenderTexture.active=null;
    var samples=new Color[3];for(int i=0;i<3;i++)samples[i]=image.GetPixel((int)((.17f+i*.33f)*image.width),image.height/2);
    string capture="ProbeOutput/"+names[index]+".png";File.WriteAllBytes(capture,image.EncodeToPNG());UnityEngine.Object.DestroyImmediate(image);
    Color[][] expected={new[]{Color.red,Color.black,Color.black},new[]{Color.black,Color.red,Color.black},new[]{Color.black,Color.black,Color.red},new[]{Color.red,Color.green,Color.blue},new[]{Color.red,Color.black,Color.black},new[]{Color.blue,Color.black,Color.black},new[]{Color.red,Color.blue,Color.black},new[]{Color.black,Color.black,Color.red},new[]{new Color(.25f,.25f,.25f,1),Color.black,Color.black}};
    if(callbacks<3 || messages.Count!=0)throw new Exception("BRG callback/warning acceptance failed: "+names[index]);
    for(int q=0;q<3;q++)for(int c=0;c<3;c++)if(Mathf.Abs(samples[q][c]-expected[index][q][c])>1.1f/255f)throw new Exception("BRG submesh or draw multiplicity mismatch: "+names[index]);
    report.rows.Add(new Row{name=names[index],submeshes=counts[index],draws=rules[index],callbacks=callbacks,samples=samples,capture=capture,messages=new List<string>(messages)});
    File.WriteAllText("ProbeOutput/result.json",JsonUtility.ToJson(report,true));
    index++;Frames=0;callbacks=0;messages.Clear();
    if(index==rules.Length){report.status="PASSED";File.WriteAllText("ProbeOutput/result.json",JsonUtility.ToJson(report,true));EditorApplication.update-=Tick;brg.Dispose();buffer.Dispose();EditorApplication.Exit(0);return;}
   }
   EditorApplication.QueuePlayerLoopUpdate();UnityEditorInternal.InternalEditorUtility.RepaintAllViews();
  }
  catch(Exception e){Fail(e);}
 }
 public static void Fail(Exception e){if(failed)return;failed=true;Directory.CreateDirectory("ProbeOutput");File.WriteAllText("ProbeOutput/failure.txt",e.ToString());EditorApplication.Exit(1);}
}
