// Copyright (c) Contributors to the Open 3D Engine Project.
// For complete copyright and license terms please see the LICENSE at the root of this distribution.
// SPDX-License-Identifier: Apache-2.0 OR MIT
// Isolated original-assembly light preparation with actual Unity culling.
// Initial records only: late cookies, atmosphere, shadows and controllers are not executed.
using System;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Collections.Generic;
using Unity.Collections;
using UnityEngine;
using UnityEngine.Rendering;
using UnityEditor;
public class FoaDirectionalPipelineAsset:RenderPipelineAsset<FoaDirectionalPipeline>
{
 protected override RenderPipeline CreatePipeline(){return new FoaDirectionalPipeline();}
}
public class FoaDirectionalPipeline:RenderPipeline
{
 protected override void Render(ScriptableRenderContext context,Camera[] cameras)
 {
  foreach(var camera in cameras){if(camera!=FoaDirectionalGpuProbe.Camera)continue;
   try{if(!camera.TryGetCullingParameters(out var parameters))throw new Exception("No culling parameters");
    var culled=context.Cull(ref parameters);context.SetupCameraProperties(camera);
    var cb=new CommandBuffer();cb.SetRenderTarget(camera.targetTexture);cb.ClearRenderTarget(true,true,Color.black);context.ExecuteCommandBuffer(cb);cb.Release();context.Submit();
    FoaDirectionalGpuProbe.Observe(culled.visibleLights);
   }catch(Exception ex){FoaDirectionalGpuProbe.Fail(ex);}
  }
 }
}
public static class FoaDirectionalGpuProbe
{
 const BindingFlags Flags=BindingFlags.Public|BindingFlags.NonPublic|BindingFlags.Instance|BindingFlags.Static;
 [Serializable] public class Value {public string name,kind;public float scalar;public int integer;public Color color;}
 [Serializable] public class Node {public Vector3 p,s;public Quaternion q;}
 [Serializable] public class Case {public string name,nativeJson;public Value[] fields;public Node[] nodes;public uint[] expectedWorldBits;}
 [Serializable] public class Input {public Case[] cases;}
 [Serializable] public class Field {public string name,type;public int offset;}
 [Serializable] public class Row {public string name,hex,recordJson,nativeBefore,nativeAfter;public Color finalColor;public uint[] worldBits,objectBits,expectedBits;public Node[] assigned;public string matrixError;public int size,count;}
 [Serializable] public class Dependency {public string path,sha256;}
 [Serializable] public class Report {public string status,error,unity,inputSha256,graphicsApi;public List<Row> rows=new List<Row>();public List<Field> fields=new List<Field>();public List<Dependency> dependencies=new List<Dependency>();}
 public static Camera Camera;static Light light;static object hd,db;static Assembly assembly;static Report report;static Input input;static GameObject rootObject;static FoaDirectionalPipelineAsset pipeline;static RenderTexture target;static string managed,inputPath,outputPath;static RenderPipelineAsset previousDefault,previousQuality;static bool previousLinear,previousTemperature;static int index,frames;static double started;static bool failing;static Dictionary<string,string> hashes=new Dictionary<string,string>();
 static string Hash(string path){using(var hash=SHA256.Create())using(var file=File.OpenRead(path))return BitConverter.ToString(hash.ComputeHash(file)).Replace("-","").ToLowerInvariant();}
 static Type T(string n){return assembly.GetType("UnityEngine.Rendering.HighDefinition."+n,true);}
 static void Set(object obj,string key,object value){obj.GetType().GetField(key,Flags).SetValue(obj,value);}
 static object Get(object obj,string key){return obj.GetType().GetField(key,Flags).GetValue(obj);}
 static object Item(object array,int n){return array.GetType().GetProperty("Item").GetValue(array,new object[]{n});}
 static object ArrayFor(Type element,int size,List<IDisposable> owned){var a=Activator.CreateInstance(typeof(NativeArray<>).MakeGenericType(element),new object[]{size,Allocator.Persistent,NativeArrayOptions.ClearMemory});owned.Add((IDisposable)a);return a;}
 static void Put(object array,int n,object value){array.GetType().GetProperty("Item").SetValue(array,value,new object[]{n});}
 public static void Run()
 {
  managed=Path.GetFullPath(Environment.GetEnvironmentVariable("FOA_LIGHT_MANAGED"));
  inputPath=Path.GetFullPath(Environment.GetEnvironmentVariable("FOA_DIRECTIONAL_INPUT"));
  outputPath=Path.GetFullPath(Environment.GetEnvironmentVariable("FOA_DIRECTIONAL_OUTPUT"));
  PrivatePath(outputPath);PrivatePath(Application.dataPath);PrivatePath(inputPath);
  if(File.Exists(outputPath) || new FileInfo(inputPath).Length>16*1024*1024)throw new Exception("Unused private output and bounded input required");
  previousDefault=GraphicsSettings.defaultRenderPipeline;previousQuality=QualitySettings.renderPipeline;
  previousLinear=GraphicsSettings.lightsUseLinearIntensity;previousTemperature=GraphicsSettings.lightsUseColorTemperature;
  report=new Report{status="PARTIAL",unity=Application.unityVersion,inputSha256=Hash(inputPath),graphicsApi=SystemInfo.graphicsDeviceType.ToString()};
  try{
   if(Application.unityVersion!="6000.0.64f1")throw new Exception("Wrong Unity version");
   var path=Path.Combine(managed,"Unity.RenderPipelines.HighDefinition.Runtime.dll");hashes[path]=Hash(path);if(hashes[path]!="ff37e967ea27e617bcd6a1987c2d9d1ad2ea29533c054323e2b12574a2d6bd63")throw new Exception("Wrong HDRP assembly");
   AppDomain.CurrentDomain.AssemblyResolve+=(sender,args)=>{var name=new AssemblyName(args.Name).Name;if(name!=Path.GetFileName(name))throw new Exception("Invalid assembly name");var a=AppDomain.CurrentDomain.GetAssemblies().FirstOrDefault(x=>x.GetName().Name==name);if(a!=null)return a;var dependency=Path.Combine(managed,name+".dll");if(!File.Exists(dependency))return null;hashes[dependency]=Hash(dependency);return Assembly.LoadFrom(dependency);};
   assembly=Assembly.LoadFrom(path);input=JsonUtility.FromJson<Input>(File.ReadAllText(inputPath));if(input.cases==null || input.cases.Length<1 || input.cases.Length>256)throw new Exception("Wrong case count");
   pipeline=ScriptableObject.CreateInstance<FoaDirectionalPipelineAsset>();GraphicsSettings.defaultRenderPipeline=pipeline;QualitySettings.renderPipeline=pipeline;
   GraphicsSettings.lightsUseLinearIntensity=true;GraphicsSettings.lightsUseColorTemperature=true;
   Camera=new GameObject("FoaLightReferenceCamera").AddComponent<Camera>();Camera.transform.position=new Vector3(100,200,300);target=new RenderTexture(64,64,24,RenderTextureFormat.ARGB32);target.Create();Camera.targetTexture=target;
   Next();started=EditorApplication.timeSinceStartup;EditorApplication.update+=Tick;
  }catch(Exception ex){Fail(ex);}
 }
 static void Assign(Transform transform,Node node)
 {
  using(var serialized=new SerializedObject(transform))
  {
   var p=serialized.FindProperty("m_LocalPosition");p.FindPropertyRelative("x").floatValue=node.p.x;p.FindPropertyRelative("y").floatValue=node.p.y;p.FindPropertyRelative("z").floatValue=node.p.z;
   var q=serialized.FindProperty("m_LocalRotation");q.FindPropertyRelative("x").floatValue=node.q.x;q.FindPropertyRelative("y").floatValue=node.q.y;q.FindPropertyRelative("z").floatValue=node.q.z;q.FindPropertyRelative("w").floatValue=node.q.w;
   var scale=serialized.FindProperty("m_LocalScale");scale.FindPropertyRelative("x").floatValue=node.s.x;scale.FindPropertyRelative("y").floatValue=node.s.y;scale.FindPropertyRelative("z").floatValue=node.s.z;
   serialized.ApplyModifiedPropertiesWithoutUndo();
  }
 }
 static void Next()
 {
  if(rootObject!=null)UnityEngine.Object.DestroyImmediate(rootObject);var c=input.cases[index];Transform parent=null;
  if(c.nodes==null || c.nodes.Length<1 || c.nodes.Length>256)throw new Exception("Invalid hierarchy depth");
  foreach(var n in c.nodes){var go=new GameObject("CapturedTransform");if(parent==null){rootObject=go;rootObject.SetActive(false);}go.transform.SetParent(parent,false);Assign(go.transform,n);parent=go.transform;}
  if(parent==null)throw new Exception("Missing captured hierarchy");light=parent.gameObject.AddComponent<Light>();hd=parent.gameObject.AddComponent(T("HDAdditionalLightData"));((Behaviour)hd).enabled=false;
  var native=JsonUtility.FromJson<FoaLightMigrationProbe.NativeWrapper>(c.nativeJson).Light;
  if(native==null || native.m_Type!=1 || !native.m_Enabled)throw new Exception("Fixture requires an enabled Directional light");
  light.type=(LightType)native.m_Type;light.enabled=native.m_Enabled;light.color=native.m_Color;light.range=native.m_Range;light.spotAngle=native.m_SpotAngle;light.innerSpotAngle=native.m_InnerSpotAngle;light.lightUnit=(UnityEngine.Rendering.LightUnit)native.m_LightUnit;light.luxAtDistance=native.m_LuxAtDistance;light.enableSpotReflector=native.m_EnableSpotReflector;light.colorTemperature=native.m_ColorTemperature;light.useColorTemperature=native.m_UseColorTemperature;light.intensity=native.m_Intensity;
  var expectedFields=new HashSet<string>("m_LightlayersMask m_FadeDistance m_Distance m_AngularDiameter m_VolumetricFadeDistance m_IncludeForRayTracing m_IncludeForPathTracing m_UseScreenSpaceShadows m_NonLightmappedOnly m_UseRayTracedShadows m_ColorShadow m_LightDimmer m_VolumetricDimmer m_ShadowDimmer m_ShadowFadeDistance m_VolumetricShadowDimmer m_ShapeWidth m_ShapeHeight m_AspectRatio m_InnerSpotPercent m_SpotIESCutoffPercent m_ShapeRadius m_BarnDoorLength useVolumetric m_AffectDiffuse m_AffectSpecular m_ApplyRangeAttenuation m_PenumbraTint m_InteractsWithSky m_ShadowTint".Split(' '));
  if(c.fields==null || c.fields.Length!=expectedFields.Count || c.expectedWorldBits==null || c.expectedWorldBits.Length!=16)throw new Exception("Incomplete fixture fields");
  foreach(var value in c.fields){if(!expectedFields.Remove(value.name))throw new Exception("Duplicate or unqualified source field");var field=hd.GetType().GetField(value.name,Flags);if(field==null)throw new Exception("Missing original field: "+value.name);object v;
   if(field.FieldType==typeof(bool)){if(value.kind!="int" || value.integer<0 || value.integer>1)throw new Exception("Invalid bool");v=value.integer==1;}
   else if(field.FieldType.IsEnum){if(value.kind!="int")throw new Exception("Invalid enum");v=Enum.ToObject(field.FieldType,value.integer);}
   else if(field.FieldType==typeof(float)){if(value.kind!="float")throw new Exception("Invalid float");v=value.scalar;}
   else if(field.FieldType==typeof(Color)){if(value.kind!="color")throw new Exception("Invalid color");v=value.color;}
   else throw new Exception("Unqualified original field: "+value.name);field.SetValue(hd,v);
  }
  rootObject.SetActive(true);hd.GetType().GetMethod("CreateHDLightRenderEntity",Flags).Invoke(hd,new object[]{false});db=T("HDLightRenderDatabase").GetProperty("instance",Flags).GetValue(null);frames=0;
 }
 public static void Observe(NativeArray<VisibleLight> visible)
 {
  if(failing || frames>=3)return;frames++;if(frames<3)return;
  try{for(int n=0;n<visible.Length;n++){if(visible[n].light==light){Process(visible[n]);return;}}throw new Exception("Source light missing from isolated cull: "+input.cases[index].name);}catch(Exception ex){Fail(ex);}
 }
 static void Process(VisibleLight visible)
 {
  var c=input.cases[index];var row=new Row{name=c.name,finalColor=visible.finalColor,nativeBefore=EditorJsonUtility.ToJson(light),worldBits=new uint[16]};
  row.objectBits=new uint[16];row.expectedBits=c.expectedWorldBits;var chain=new List<Node>();for(var tr=light.transform;tr!=null;tr=tr.parent)chain.Add(new Node{p=tr.localPosition,q=tr.localRotation,s=tr.localScale});chain.Reverse();row.assigned=chain.ToArray();
  for(int r=0;r<4;r++)for(int col=0;col<4;col++){uint bits=BitConverter.ToUInt32(BitConverter.GetBytes(visible.localToWorldMatrix[r,col]),0);row.worldBits[r*4+col]=bits;row.objectBits[r*4+col]=BitConverter.ToUInt32(BitConverter.GetBytes(light.transform.localToWorldMatrix[r,col]),0);if(row.objectBits[r*4+col]!=c.expectedWorldBits[r*4+col] && !(light.transform.localToWorldMatrix[r,col]==0 && BitConverter.ToSingle(BitConverter.GetBytes(c.expectedWorldBits[r*4+col]),0)==0))row.matrixError="Transform matrix differs from captured source hierarchy: "+c.name+" "+r+","+col;}
  if(row.matrixError!=null){report.rows.Add(row);throw new Exception(row.matrixError);}
  var owned=new List<IDisposable>();try{
   var dataIndex=(int)db.GetType().GetMethod("GetEntityDataIndex",Flags).Invoke(db,new[]{Get(hd,"lightEntity")});var data=db.GetType().GetProperty("lightData",Flags).GetValue(db);
   var jobType=T("HDGpuLightsBuilder").GetNestedType("CreateGpuLightDataJob",Flags);var job=Activator.CreateInstance(jobType);var global=Activator.CreateInstance(jobType.GetField("globalConfig",Flags).FieldType);Set(global,"lightLayersEnabled",true);Set(global,"specularGlobalDimmer",1f);Set(global,"invalidScreenSpaceShadowIndex",(int)(uint)T("LightDefinitions").GetField("s_InvalidScreenSpaceShadow",Flags).GetValue(null));Set(job,"globalConfig",global);
   Set(job,"cameraPos",Camera.transform.position);Set(job,"useCameraRelativePosition",true);Set(job,"defaultDataIndex",-1);Set(job,"isPbrSkyActive",true);Set(job,"lightRenderDataArray",data);
   var processed=Activator.CreateInstance(T("HDProcessedVisibleLight"));Set(processed,"dataIndex",dataIndex);var processedArray=ArrayFor(processed.GetType(),1,owned);Put(processedArray,0,processed);Set(job,"processedEntities",processedArray);
   var visibleArray=ArrayFor(typeof(VisibleLight),1,owned);Put(visibleArray,0,visible);Set(job,"visibleLights",visibleArray);var output=ArrayFor(T("DirectionalLightData"),1,owned);Set(job,"directionalLights",output);var counts=ArrayFor(typeof(int),3,owned);Set(job,"gpuLightCounters",counts);
   var method=jobType.GetMethod("ConvertDirectionalLightToGPUFormat",Flags);var parameters=method.GetParameters();method.Invoke(job,new object[]{0,0,Enum.ToObject(parameters[2].ParameterType,0),Enum.ToObject(parameters[3].ParameterType,0),Enum.ToObject(parameters[4].ParameterType,0)});
   var result=Item(output,0);row.size=Marshal.SizeOf(result);row.count=(int)Item(counts,0);row.recordJson=JsonUtility.ToJson(result);var ptr=Marshal.AllocHGlobal(row.size);try{Marshal.StructureToPtr(result,ptr,false);var bytes=new byte[row.size];Marshal.Copy(ptr,bytes,0,bytes.Length);row.hex=BitConverter.ToString(bytes).Replace("-","").ToLowerInvariant();}finally{Marshal.FreeHGlobal(ptr);}
   if(report.fields.Count==0)foreach(var field in result.GetType().GetFields(Flags)){if(!field.IsStatic)report.fields.Add(new Field{name=field.Name,type=field.FieldType.FullName,offset=(int)Marshal.OffsetOf(result.GetType(),field.Name)});}
   row.nativeAfter=EditorJsonUtility.ToJson(light);if(row.nativeBefore!=row.nativeAfter)throw new Exception("Builder mutated the native Light");if(row.size!=176 || row.count!=1)throw new Exception("Invalid directional GPU result");report.rows.Add(row);
  }finally{foreach(var array in owned)array.Dispose();}
 }
 static void Tick()
 {
  if(failing)return;try{
   if(EditorApplication.timeSinceStartup-started>180)throw new Exception("GPU-light probe timeout");
   if(frames>=3 && report.rows.Count==index+1){index++;if(index==input.cases.Length){Finish();return;}Next();}
   EditorApplication.QueuePlayerLoopUpdate();UnityEditorInternal.InternalEditorUtility.RepaintAllViews();
  }catch(Exception ex){Fail(ex);}
 }
 static void Finish(){if(Hash(inputPath)!=report.inputSha256)throw new Exception("Input changed");foreach(var pair in hashes){if(Hash(pair.Key)!=pair.Value)throw new Exception("Assembly changed");report.dependencies.Add(new Dependency{path=pair.Key,sha256=pair.Value});}report.status="PASSED";Write();EditorApplication.update-=Tick;EditorApplication.Exit(0);}
 static void PrivatePath(string path)
 {
  for(var directory=new DirectoryInfo(Path.GetDirectoryName(path));directory!=null;directory=directory.Parent)
   if(File.Exists(Path.Combine(directory.FullName,".git")) || Directory.Exists(Path.Combine(directory.FullName,".git")))throw new Exception("Private fixture must be outside source control");
  var install=Directory.GetParent(managed).Parent.FullName+Path.DirectorySeparatorChar;
  if(path.StartsWith(install,StringComparison.OrdinalIgnoreCase))throw new Exception("Private fixture must be outside the game installation");
 }
 static void Cleanup()
 {
  GraphicsSettings.defaultRenderPipeline=previousDefault;QualitySettings.renderPipeline=previousQuality;
  GraphicsSettings.lightsUseLinearIntensity=previousLinear;GraphicsSettings.lightsUseColorTemperature=previousTemperature;
  if(rootObject!=null)UnityEngine.Object.DestroyImmediate(rootObject);
  if(Camera!=null)UnityEngine.Object.DestroyImmediate(Camera.gameObject);
  if(target!=null){target.Release();UnityEngine.Object.DestroyImmediate(target);}
  if(pipeline!=null)UnityEngine.Object.DestroyImmediate(pipeline);
 }
 static void Write()
 {
  using(var stream=new FileStream(outputPath,FileMode.CreateNew,FileAccess.Write))
  using(var writer=new StreamWriter(stream))writer.Write(JsonUtility.ToJson(report,true));
  Cleanup();
 }
 public static void Fail(Exception ex){if(failing)return;failing=true;report.status="FAILED";report.error=ex.ToString();Write();EditorApplication.update-=Tick;EditorApplication.Exit(1);}
}
