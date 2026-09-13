// Copyright (c) Contributors to the Open 3D Engine Project.
// For complete copyright and license terms please see the LICENSE at the root of this distribution.
// SPDX-License-Identifier: Apache-2.0 OR MIT
// Independent Unity Editor readback of exact, privately copied terrain Mesh assets.
using System;
using System.Collections.Generic;
using System.IO;
using System.Security.Cryptography;
using Unity.Collections;
using UnityEditor;
using UnityEngine;
using UnityEngine.Rendering;

public static class FoaTerrainMeshProbe
{
    [Serializable] public class MeshRequest { public string key, bundle, bundle_sha256, asset, subobject_name; }
    [Serializable] public class Request { public string bundle_root, output; public MeshRequest[] meshes; }
    [Serializable] public class Row { public string key, subobject_name, positions_sha256, indices_sha256; public int vertices, triangles, submeshes; }
    [Serializable] public class Receipt { public string status, unity, request_sha256; public List<Row> meshes = new List<Row>(); }
    static void Require(bool ok, string message) { if(!ok) throw new InvalidOperationException(message); }
    static string Sha(byte[] bytes) { using(var h=SHA256.Create()) return BitConverter.ToString(h.ComputeHash(bytes)).Replace("-", "").ToLowerInvariant(); }
    static string Private(string path)
    {
        Require(Path.IsPathRooted(path), "Absolute private path required.");
        path=Path.GetFullPath(path);
        for(var dir=new DirectoryInfo(path);dir!=null;dir=dir.Parent)
        {
            Require(!Directory.Exists(Path.Combine(dir.FullName,".git")) && !File.Exists(Path.Combine(dir.FullName,".git")), "Private data cannot enter source control.");
            if(dir.Exists) Require((dir.Attributes & FileAttributes.ReparsePoint)==0, "Private directory links are unsupported.");
        }
        if(File.Exists(path)) Require((File.GetAttributes(path) & FileAttributes.ReparsePoint)==0, "Private file links are unsupported.");
        return path;
    }
    // Unity 6000.0 Mesh.GetVertexBuffer exposes the original GPU upload even when
    // a built bundle deliberately omits the CPU mesh copy. Never set buffer targets
    // or rebuild that mesh: read the existing buffers only.
    static byte[] ReadGpu(GraphicsBuffer buffer)
    {
        Require(buffer!=null && buffer.IsValid() && (long)buffer.count*buffer.stride<=64*1024*1024, "Invalid native mesh GPU buffer.");
        using(buffer)
        {
            var request=AsyncGPUReadback.Request(buffer); request.WaitForCompletion();
            Require(!request.hasError, "Native terrain GPU readback failed.");
            return request.GetData<byte>().ToArray();
        }
    }
    public static void Run()
    {
        try
        {
            Require(Application.unityVersion=="6000.0.64f1", "Unqualified Unity profile.");
            var input=Private(Environment.GetEnvironmentVariable("FOA_TERRAIN_MESH_INPUT"));
            Require(new FileInfo(input).Length<=4*1024*1024, "Mesh request exceeds its bound.");
            byte[] raw=File.ReadAllBytes(input); var request=JsonUtility.FromJson<Request>(System.Text.Encoding.UTF8.GetString(raw));
            string root=Private(request.bundle_root), output=Private(request.output);
            Require(Directory.Exists(output) && Directory.GetFileSystemEntries(output).Length==0, "New empty private output required.");
            Require(request.meshes!=null && request.meshes.Length>0 && request.meshes.Length<=4096, "Invalid mesh request count.");
            var receipt=new Receipt { status="PASSED", unity=Application.unityVersion, request_sha256=Sha(raw) };
            string current=null, fingerprint=null; AssetBundle bundle=null; var seen=new HashSet<string>();
            try
            {
                foreach(var item in request.meshes)
                {
                    Require(seen.Add(item.key) && item.key.Length<=128 && item.key.IndexOfAny(Path.GetInvalidFileNameChars())<0, "Invalid or duplicate mesh key.");
                    string path=Private(Path.Combine(root,item.bundle));
                    Require(path.StartsWith(root+Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase), "Mesh source escapes private fixture.");
                    if(path!=current)
                    {
                        if(bundle!=null) { bundle.Unload(true); Require(Sha(File.ReadAllBytes(current))==fingerprint, "Mesh bundle changed."); }
                        Require(Sha(File.ReadAllBytes(path))==item.bundle_sha256, "Mesh bundle fingerprint differs.");
                        bundle=AssetBundle.LoadFromFile(path); Require(bundle!=null, "Native Unity could not load the source bundle.");
                        current=path; fingerprint=item.bundle_sha256;
                    }
                    Require(item.bundle_sha256==fingerprint, "Conflicting bundle fingerprint.");
                    var candidates=bundle.LoadAssetWithSubAssets<Mesh>(item.asset);
                    var matches=Array.FindAll(candidates, value=>value.name==item.subobject_name);
                    Require(matches.Length==1, "Exact source Mesh subobject is absent or ambiguous."); var mesh=matches[0];
                    Debug.Log("Terrain mesh readback: "+item.key+" readable="+mesh.isReadable);
                    {
                        Require(mesh.vertexCount>=3 && mesh.vertexCount<=1000000 && mesh.subMeshCount>0 && mesh.subMeshCount<=4096, "Native terrain mesh exceeds its bounds.");
                        Require(mesh.GetVertexAttributeFormat(VertexAttribute.Position)==VertexAttributeFormat.Float32 && mesh.GetVertexAttributeDimension(VertexAttribute.Position)==3, "Unqualified native terrain position format.");
                        int streamIndex=mesh.GetVertexAttributeStream(VertexAttribute.Position);
                        int stride=mesh.GetVertexBufferStride(streamIndex), offset=mesh.GetVertexAttributeOffset(VertexAttribute.Position);
                        byte[] vertexBuffer=ReadGpu(mesh.GetVertexBuffer(streamIndex));
                        Require(stride>=12 && offset>=0 && offset+12<=stride && (long)mesh.vertexCount*stride<=vertexBuffer.Length, "Incomplete native vertex buffer.");
                        byte[] positions=new byte[mesh.vertexCount*12];
                        for(int i=0;i<mesh.vertexCount;i++) Buffer.BlockCopy(vertexBuffer,i*stride+offset,positions,i*12,12);
                        byte[] indexBuffer=ReadGpu(mesh.GetIndexBuffer()); byte[] indices; int triangles=0;
                        using(var stream=new MemoryStream()) using(var writer=new BinaryWriter(stream))
                        {
                            writer.Write(mesh.subMeshCount);
                            for(int submesh=0;submesh<mesh.subMeshCount;submesh++)
                            {
                                var sub=mesh.GetSubMesh(submesh); Require(sub.topology==MeshTopology.Triangles && sub.indexCount%3==0, "Non-triangle terrain source.");
                                writer.Write(sub.indexCount); triangles+=sub.indexCount/3;
                                int width=mesh.indexFormat==IndexFormat.UInt16?2:4;
                                Require((long)(sub.indexStart+sub.indexCount)*width<=indexBuffer.Length, "Incomplete native index buffer.");
                                for(int i=0;i<sub.indexCount;i++)
                                {
                                    int at=(sub.indexStart+i)*width;
                                    uint index=width==2?BitConverter.ToUInt16(indexBuffer,at):BitConverter.ToUInt32(indexBuffer,at);
                                    writer.Write(checked(index+(uint)sub.baseVertex));
                                }
                            }
                            indices=stream.ToArray();
                        }
                        File.WriteAllBytes(Path.Combine(output,item.key+".positions"),positions);
                        File.WriteAllBytes(Path.Combine(output,item.key+".indices"),indices);
                        receipt.meshes.Add(new Row { key=item.key, subobject_name=mesh.name, vertices=mesh.vertexCount, triangles=triangles, submeshes=mesh.subMeshCount,
                            positions_sha256=Sha(positions), indices_sha256=Sha(indices) });
                    }
                }
            }
            finally { if(bundle!=null) { bundle.Unload(true); Require(Sha(File.ReadAllBytes(current))==fingerprint, "Mesh source changed."); } }
            Require(Sha(File.ReadAllBytes(input))==receipt.request_sha256, "Mesh request changed.");
            File.WriteAllText(Path.Combine(output,"receipt.json"),JsonUtility.ToJson(receipt,true)); EditorApplication.Exit(0);
        }
        catch(Exception error) { Debug.LogException(error); EditorApplication.Exit(2); }
    }
}
