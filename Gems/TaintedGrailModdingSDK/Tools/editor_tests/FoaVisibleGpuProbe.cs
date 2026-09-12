// Copyright (c) Contributors to the Open 3D Engine Project.
// For complete copyright and license terms please see the LICENSE at the root of this distribution.
// SPDX-License-Identifier: Apache-2.0 OR MIT
// Isolated original-assembly light preparation with actual Unity culling.
// Original visible-light processing and initial non-directional records only; no scene occluders or live controllers.
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
public class FoaVisiblePipelineAsset:RenderPipelineAsset<FoaVisiblePipeline>
{
 protected override RenderPipeline CreatePipeline(){return new FoaVisiblePipeline();}
}
public class FoaVisiblePipeline:RenderPipeline
{
 protected override void Render(ScriptableRenderContext context,Camera[] cameras)
 {
  foreach(var camera in cameras){if(camera!=FoaVisibleGpuProbe.Camera)continue;
   try{if(!camera.TryGetCullingParameters(out var parameters))throw new Exception("No culling parameters");
    var culled=context.Cull(ref parameters);context.SetupCameraProperties(camera);
    var cb=new CommandBuffer();cb.SetRenderTarget(camera.targetTexture);cb.ClearRenderTarget(true,true,Color.black);context.ExecuteCommandBuffer(cb);cb.Release();context.Submit();
    FoaVisibleGpuProbe.Observe(culled.visibleLights);
   }catch(Exception ex){FoaVisibleGpuProbe.Fail(ex);}
  }
 }
}
public static class FoaVisibleGpuProbe
{
 const BindingFlags Flags=BindingFlags.Public|BindingFlags.NonPublic|BindingFlags.Instance|BindingFlags.Static;
 [Serializable] public class Value {public string name,kind;public float scalar;public int integer;public Color color;}
 [Serializable] public class Node {public Vector3 p,s;public Quaternion q;public bool active;}
 [Serializable] public class Case {public string name,nativeJson;public Value[] fields;public Node[] nodes;public uint[] expectedWorldBits;public Extra extra;public bool expectedActive;}
 [Serializable] public class Baking {public int probeOcclusionLightIndex,occlusionMaskChannel,lightmapBakeType,mixedLightingMode;public bool isBaked;}
 [Serializable] public class Extra {public int layer,shadows,caster,renderMode,lightmapping;public uint cullingMask,renderingLayerMask;public bool forceVisible,useBoundingSphere,useViewFrustum;public uint[] boundingSphereBits;public Vector2 areaSize;public Baking baking;}
 [Serializable] public class Input {public Case[] cases;}
 [Serializable] public class Field {public string name,type;public int offset;}
 [Serializable] public class Row {public string name,hex,recordJson,nativeBefore,nativeAfter;public Color finalColor;public uint[] worldBits,objectBits,expectedBits;public Node[] assigned;public string matrixError;public int size,count;public bool culled,active;public Baking bakingBefore,bakingAfter;public uint[] boundsBefore,boundsAfter;public int layer;public string processed,renderData;public int[] counters;public Vector3 camera;}
 [Serializable] public class Dependency {public string path,sha256;}
 [Serializable] public class Report {public string status,error,unity,inputSha256,graphicsApi;public List<Row> rows=new List<Row>();public List<Field> fields=new List<Field>();public List<Dependency> dependencies=new List<Dependency>();}
 public static Camera Camera;static Light light;static object hd,db;static Assembly assembly;static Report report;static Input input;static GameObject rootObject;static FoaVisiblePipelineAsset pipeline;static RenderTexture target;static string managed,inputPath,outputPath;static RenderPipelineAsset previousDefault,previousQuality;static bool previousLinear,previousTemperature;static int index,frames;static double started;static bool failing;static Dictionary<string,string> hashes=new Dictionary<string,string>();
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
  inputPath=Path.GetFullPath(Environment.GetEnvironmentVariable("FOA_VISIBLE_INPUT"));
  outputPath=Path.GetFullPath(Environment.GetEnvironmentVariable("FOA_VISIBLE_OUTPUT"));
  PrivatePath(outputPath);PrivatePath(Application.dataPath);PrivatePath(inputPath);
  if(File.Exists(outputPath) || new FileInfo(inputPath).Length>32*1024*1024)throw new Exception("Unused private output and bounded input required");
  previousDefault=GraphicsSettings.defaultRenderPipeline;previousQuality=QualitySettings.renderPipeline;
  previousLinear=GraphicsSettings.lightsUseLinearIntensity;previousTemperature=GraphicsSettings.lightsUseColorTemperature;
  report=new Report{status="PARTIAL",unity=Application.unityVersion,inputSha256=Hash(inputPath),graphicsApi=SystemInfo.graphicsDeviceType.ToString()};
  try{
   if(Application.unityVersion!="6000.0.64f1")throw new Exception("Wrong Unity version");
   var path=Path.Combine(managed,"Unity.RenderPipelines.HighDefinition.Runtime.dll");hashes[path]=Hash(path);if(hashes[path]!="ff37e967ea27e617bcd6a1987c2d9d1ad2ea29533c054323e2b12574a2d6bd63")throw new Exception("Wrong HDRP assembly");
   AppDomain.CurrentDomain.AssemblyResolve+=(sender,args)=>{var name=new AssemblyName(args.Name).Name;if(name!=Path.GetFileName(name))throw new Exception("Invalid assembly name");var a=AppDomain.CurrentDomain.GetAssemblies().FirstOrDefault(x=>x.GetName().Name==name);if(a!=null)return a;var dependency=Path.Combine(managed,name+".dll");if(!File.Exists(dependency))return null;hashes[dependency]=Hash(dependency);return Assembly.LoadFrom(dependency);};
   assembly=Assembly.LoadFrom(path);input=JsonUtility.FromJson<Input>(File.ReadAllText(inputPath));if(input.cases==null || input.cases.Length<1 || input.cases.Length>4096)throw new Exception("Wrong case count");
   pipeline=ScriptableObject.CreateInstance<FoaVisiblePipelineAsset>();GraphicsSettings.defaultRenderPipeline=pipeline;QualitySettings.renderPipeline=pipeline;
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
  foreach(var n in c.nodes){var go=new GameObject("CapturedTransform");if(parent==null){rootObject=go;rootObject.SetActive(false);}go.transform.SetParent(parent,false);Assign(go.transform,n);if(parent!=null)go.SetActive(n.active);parent=go.transform;}
  if(parent==null)throw new Exception("Missing captured hierarchy");light=parent.gameObject.AddComponent<Light>();hd=parent.gameObject.AddComponent(T("HDAdditionalLightData"));((Behaviour)hd).enabled=false;
  var native=JsonUtility.FromJson<FoaLightMigrationProbe.NativeWrapper>(c.nativeJson).Light;
  if(native==null || native.m_Type==1 || !native.m_Enabled)throw new Exception("Fixture requires an enabled non-directional light");
  light.type=(LightType)native.m_Type;light.enabled=native.m_Enabled;light.color=native.m_Color;light.range=native.m_Range;light.spotAngle=native.m_SpotAngle;light.innerSpotAngle=native.m_InnerSpotAngle;light.lightUnit=(UnityEngine.Rendering.LightUnit)native.m_LightUnit;light.luxAtDistance=native.m_LuxAtDistance;light.enableSpotReflector=native.m_EnableSpotReflector;light.colorTemperature=native.m_ColorTemperature;light.useColorTemperature=native.m_UseColorTemperature;light.intensity=native.m_Intensity;
  var expectedFields=new HashSet<string>("m_LightlayersMask m_FadeDistance m_Distance m_AngularDiameter m_VolumetricFadeDistance m_IncludeForRayTracing m_IncludeForPathTracing m_UseScreenSpaceShadows m_NonLightmappedOnly m_UseRayTracedShadows m_ColorShadow m_LightDimmer m_VolumetricDimmer m_ShadowDimmer m_ShadowFadeDistance m_VolumetricShadowDimmer m_ShapeWidth m_ShapeHeight m_AspectRatio m_InnerSpotPercent m_SpotIESCutoffPercent m_ShapeRadius m_BarnDoorLength m_BarnDoorAngle useVolumetric m_AffectDiffuse m_AffectSpecular m_ApplyRangeAttenuation m_PenumbraTint m_InteractsWithSky m_ShadowTint".Split(' '));
  if(c.fields==null || c.fields.Length!=expectedFields.Count || c.expectedWorldBits==null || c.expectedWorldBits.Length!=16)throw new Exception("Incomplete fixture fields");
  foreach(var value in c.fields){if(!expectedFields.Remove(value.name))throw new Exception("Duplicate or unqualified source field");var field=hd.GetType().GetField(value.name,Flags);if(field==null)throw new Exception("Missing original field: "+value.name);object v;
   if(field.FieldType==typeof(bool)){if(value.kind!="int" || value.integer<0 || value.integer>1)throw new Exception("Invalid bool");v=value.integer==1;}
   else if(field.FieldType.IsEnum){if(value.kind!="int")throw new Exception("Invalid enum");v=Enum.ToObject(field.FieldType,value.integer);}
   else if(field.FieldType==typeof(float)){if(value.kind!="float")throw new Exception("Invalid float");v=value.scalar;}
   else if(field.FieldType==typeof(Color)){if(value.kind!="color")throw new Exception("Invalid color");v=value.color;}
   else throw new Exception("Unqualified original field: "+value.name);field.SetValue(hd,v);
  }
  if(c.extra==null)throw new Exception("Missing native visibility fields");var e=c.extra;
  light.gameObject.layer=e.layer;light.shadows=(LightShadows)e.shadows;light.lightShadowCasterMode=(LightShadowCasterMode)e.caster;
  light.renderMode=(LightRenderMode)e.renderMode;light.cullingMask=unchecked((int)e.cullingMask);light.renderingLayerMask=unchecked((int)e.renderingLayerMask);
  light.areaSize=e.areaSize;light.forceVisible=e.forceVisible;if(e.boundingSphereBits==null || e.boundingSphereBits.Length!=4)throw new Exception("Missing exact bounding-sphere bits");light.boundingSphereOverride=new Vector4(Float(e.boundingSphereBits[0]),Float(e.boundingSphereBits[1]),Float(e.boundingSphereBits[2]),Float(e.boundingSphereBits[3]));light.useBoundingSphereOverride=e.useBoundingSphere;light.useViewFrustumForShadowCasterCull=e.useViewFrustum;
  using(var serialized=new SerializedObject(light)){serialized.FindProperty("m_Lightmapping").intValue=e.lightmapping;serialized.ApplyModifiedPropertiesWithoutUndo();}
  light.bakingOutput=new LightBakingOutput{probeOcclusionLightIndex=e.baking.probeOcclusionLightIndex,occlusionMaskChannel=e.baking.occlusionMaskChannel,lightmapBakeType=(LightmapBakeType)e.baking.lightmapBakeType,mixedLightingMode=(MixedLightingMode)e.baking.mixedLightingMode,isBaked=e.baking.isBaked};
  rootObject.SetActive(c.nodes[0].active);if(light.isActiveAndEnabled!=c.expectedActive)throw new Exception("Source enabled hierarchy changed");
  Camera.transform.position=light.transform.position-light.transform.forward*Mathf.Max(1f,light.range*.25f);Camera.transform.rotation=light.transform.rotation;
  Camera.nearClipPlane=.01f;Camera.farClipPlane=Mathf.Max(1000f,light.range*4);Camera.fieldOfView=60;Camera.cullingMask=-1;
  hd.GetType().GetMethod("CreateHDLightRenderEntity",Flags).Invoke(hd,new object[]{false});db=T("HDLightRenderDatabase").GetProperty("instance",Flags).GetValue(null);frames=0;
 }
 public static void Observe(NativeArray<VisibleLight> visible)
 {
  if(failing || frames>=3)return;frames++;if(frames<3)return;
  try{for(int n=0;n<visible.Length;n++){if(visible[n].light==light){Process(visible[n]);return;}}Process(default(VisibleLight),false);}catch(Exception ex){Fail(ex);}
 }
 static float Float(uint bits){return BitConverter.ToSingle(BitConverter.GetBytes(bits),0);}
 static uint[] BoundsOf(Light light){var b=light.boundingSphereOverride;return new[]{b.x,b.y,b.z,b.w}.Select(f=>BitConverter.ToUInt32(BitConverter.GetBytes(f),0)).ToArray();}
 static Baking BakeOf(Light light){var b=light.bakingOutput;return new Baking{probeOcclusionLightIndex=b.probeOcclusionLightIndex,occlusionMaskChannel=b.occlusionMaskChannel,lightmapBakeType=(int)b.lightmapBakeType,mixedLightingMode=(int)b.mixedLightingMode,isBaked=b.isBaked};}
 static void Process(VisibleLight visible,bool culled=true)
 {
  var c=input.cases[index];var row=new Row{bakingBefore=BakeOf(light),boundsBefore=BoundsOf(light),layer=light.gameObject.layer,culled=culled,active=light.isActiveAndEnabled,camera=Camera.transform.position,name=c.name,finalColor=visible.finalColor,nativeBefore=EditorJsonUtility.ToJson(light),worldBits=new uint[16]};
  row.objectBits=new uint[16];row.expectedBits=c.expectedWorldBits;var chain=new List<Node>();for(var tr=light.transform;tr!=null;tr=tr.parent)chain.Add(new Node{p=tr.localPosition,q=tr.localRotation,s=tr.localScale,active=tr.gameObject.activeSelf});chain.Reverse();row.assigned=chain.ToArray();
  for(int r=0;r<4;r++)for(int col=0;col<4;col++){uint bits=BitConverter.ToUInt32(BitConverter.GetBytes(visible.localToWorldMatrix[r,col]),0);row.worldBits[r*4+col]=bits;row.objectBits[r*4+col]=BitConverter.ToUInt32(BitConverter.GetBytes(light.transform.localToWorldMatrix[r,col]),0);if(row.objectBits[r*4+col]!=c.expectedWorldBits[r*4+col] && !(light.transform.localToWorldMatrix[r,col]==0 && BitConverter.ToSingle(BitConverter.GetBytes(c.expectedWorldBits[r*4+col]),0)==0))row.matrixError="Transform matrix differs from captured source hierarchy: "+c.name+" "+r+","+col;}
  if(row.matrixError!=null){report.rows.Add(row);throw new Exception(row.matrixError);}
  if(!culled){row.bakingAfter=BakeOf(light);row.boundsAfter=BoundsOf(light);row.nativeAfter=EditorJsonUtility.ToJson(light);report.rows.Add(row);return;}
  if(!c.expectedActive)throw new Exception("Inactive source light entered native cull");
  var owned=new List<IDisposable>();try{
   var dataIndex=(int)db.GetType().GetMethod("GetEntityDataIndex",Flags).Invoke(db,new[]{Get(hd,"lightEntity")});var data=db.GetType().GetProperty("lightData",Flags).GetValue(db);row.renderData=JsonUtility.ToJson(Item(data,dataIndex));
   var visibleArray=ArrayFor(typeof(VisibleLight),1,owned);Put(visibleArray,0,visible);
   var indexArray=ArrayFor(typeof(int),1,owned);Put(indexArray,0,dataIndex);
   var baking=ArrayFor(typeof(LightBakingOutput),1,owned);Put(baking,0,light.bakingOutput);
   var shadows=ArrayFor(typeof(LightShadows),1,owned);Put(shadows,0,light.shadows);
   var processed=ArrayFor(T("HDProcessedVisibleLight"),1,owned);var volumes=ArrayFor(T("LightVolumeType"),1,owned);
   var counters=ArrayFor(typeof(int),6,owned);var keys=ArrayFor(typeof(uint),1,owned);var shadowIndices=ArrayFor(typeof(int),1,owned);
   var processType=T("HDProcessedVisibleLightsBuilder").GetNestedType("ProcessVisibleLightJob",Flags);var process=Activator.CreateInstance(processType);
   Set(process,"lightData",data);Set(process,"visibleLights",visibleArray);Set(process,"visibleLightEntityDataIndices",indexArray);Set(process,"visibleLightBakingOutput",baking);Set(process,"visibleLightShadows",shadows);Set(process,"totalLightCounts",1);
   Set(process,"cameraPosition",Activator.CreateInstance(processType.GetField("cameraPosition",Flags).FieldType,new object[]{Camera.transform.position.x,Camera.transform.position.y,Camera.transform.position.z}));
   Set(process,"pixelCount",Camera.pixelWidth*Camera.pixelHeight);
   foreach(var key in new[]{"enableAreaLights","showDirectionalLight","showPunctualLight","showAreaLight","enableShadowMaps"})Set(process,key,true);
   foreach(var key in new[]{"enableRayTracing","enablePathTracing","enableScreenSpaceShadows"})Set(process,key,false);
   foreach(var key in new[]{"maxDirectionalLightsOnScreen","maxPunctualLightsOnScreen","maxAreaLightsOnScreen"})Set(process,key,4096);
   Set(process,"debugFilterMode",Enum.ToObject(processType.GetField("debugFilterMode",Flags).FieldType,0));
   Set(process,"processedVisibleLightCountsPtr",counters);Set(process,"processedLightVolumeType",volumes);Set(process,"processedEntities",processed);Set(process,"sortKeys",keys);Set(process,"shadowLightsDataIndices",shadowIndices);
   processType.GetMethod("Execute",Flags).Invoke(process,new object[]{0});row.counters=Enumerable.Range(0,6).Select(n=>(int)Item(counters,n)).ToArray();
   if(row.counters[0]<0 || row.counters[0]>1)throw new Exception("Invalid processed count");
   if(row.counters[0]==1){
    var selected=Item(processed,0);row.processed=JsonUtility.ToJson(selected);
    var jobType=T("HDGpuLightsBuilder").GetNestedType("CreateGpuLightDataJob",Flags);var job=Activator.CreateInstance(jobType);var global=Activator.CreateInstance(jobType.GetField("globalConfig",Flags).FieldType);
    Set(global,"lightLayersEnabled",true);Set(global,"specularGlobalDimmer",1f);Set(global,"maxShadowFadeDistance",10000f);Set(global,"invalidScreenSpaceShadowIndex",(int)(uint)T("LightDefinitions").GetField("s_InvalidScreenSpaceShadow",Flags).GetValue(null));Set(job,"globalConfig",global);
    Set(job,"cameraPos",Camera.transform.position);Set(job,"useCameraRelativePosition",true);Set(job,"viewCounts",0);Set(job,"defaultDataIndex",-1);Set(job,"lightRenderDataArray",data);Set(job,"processedEntities",processed);Set(job,"visibleLights",visibleArray);Set(job,"visibleLightBakingOutput",baking);
    var casters=ArrayFor(typeof(LightShadowCasterMode),1,owned);Put(casters,0,light.lightShadowCasterMode);Set(job,"visibleLightShadowCasterMode",casters);
    var output=ArrayFor(T("LightData"),1,owned);Set(job,"lights",output);var gpuCounters=ArrayFor(typeof(int),3,owned);Set(job,"gpuLightCounters",gpuCounters);
    var evaluate=T("HDRenderPipeline").GetMethod("EvaluateGPULightType",Flags);var ep=evaluate.GetParameters();var eval=new object[]{light.type,Activator.CreateInstance(ep[1].ParameterType.GetElementType()),Activator.CreateInstance(ep[2].ParameterType.GetElementType()),Activator.CreateInstance(ep[3].ParameterType.GetElementType())};evaluate.Invoke(null,eval);
    if(!eval[2].Equals(Get(selected,"gpuLightType")) || !eval[3].Equals(Item(volumes,0)))throw new Exception("Original classification disagrees");
    jobType.GetMethod("StoreAndConvertLightToGPUFormat",Flags).Invoke(job,new object[]{0,0,eval[1],eval[2],eval[3],false});
    var result=Item(output,0);row.size=Marshal.SizeOf(result);row.count=(int)Item(gpuCounters,1)+(int)Item(gpuCounters,2);row.recordJson=JsonUtility.ToJson(result);
    var ptr=Marshal.AllocHGlobal(row.size);try{Marshal.StructureToPtr(result,ptr,false);var bytes=new byte[row.size];Marshal.Copy(ptr,bytes,0,bytes.Length);row.hex=BitConverter.ToString(bytes).Replace("-","").ToLowerInvariant();}finally{Marshal.FreeHGlobal(ptr);}
    if(report.fields.Count==0)foreach(var field in result.GetType().GetFields(Flags))if(!field.IsStatic)report.fields.Add(new Field{name=field.Name,type=field.FieldType.FullName,offset=(int)Marshal.OffsetOf(result.GetType(),field.Name)});
    if(row.count!=1)throw new Exception("Missing original GPU light");
   }
   row.bakingAfter=BakeOf(light);row.boundsAfter=BoundsOf(light);row.nativeAfter=EditorJsonUtility.ToJson(light);if(row.nativeBefore!=row.nativeAfter)throw new Exception("Original jobs mutated the native Light");report.rows.Add(row);
  }finally{foreach(var array in owned)array.Dispose();}
 }
 static void Tick()
 {
  if(failing)return;try{
   if(EditorApplication.timeSinceStartup-started>900)throw new Exception("GPU-light probe timeout");
   if(frames>=3 && report.rows.Count==index+1){index++;if(index%128==0)Debug.Log("FOA visible-light cases: "+index);if(index==input.cases.Length){Finish();return;}Next();}
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
