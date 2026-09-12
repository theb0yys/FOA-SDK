// Copyright (c) Contributors to the Open 3D Engine Project.
// For complete copyright and license terms please see the LICENSE at the root of this distribution.
// SPDX-License-Identifier: Apache-2.0 OR MIT
// Independent Unity Transform consumer of private source-bound or synthetic TRS.
// No game assemblies, scripts, scene loading, game writes or native-map claim.
using System;
using System.Collections.Generic;
using System.IO;
using System.Security.Cryptography;
using UnityEditor;
using UnityEngine;

public static class FoaTransformProbe
{
    [Serializable] public class Node { public int parent; public Vector3 p, s; public Quaternion q; public string kind; }
    [Serializable] public class Input { public Node[] nodes; }
    [Serializable] public class Row { public float[] world, local; public Vector3 p, s; public Quaternion q; public int[] inputBits, assignedBits, localBits, worldBits; }
    [Serializable] public class Output { public string status, inputSha256, unityVersion, scope; public int schemaVersion=3; public int rectTransforms; public List<Row> rows = new List<Row>(); }
    static float[] Matrix(Matrix4x4 m)
    {
        var values = new float[16];
        for(int r=0;r<4;r++) for(int c=0;c<4;c++) values[r*4+c]=m[r,c];
        return values;
    }
    static int[] MatrixBits(Matrix4x4 matrix)
    {
        var values=Matrix(matrix);var result=new int[values.Length];
        for(int i=0;i<values.Length;i++) result[i]=BitConverter.SingleToInt32Bits(values[i]);
        return result;
    }
    static int[] Bits(Vector3 p, Quaternion q, Vector3 s)
    {
        var values=new[]{p.x,p.y,p.z,q.x,q.y,q.z,q.w,s.x,s.y,s.z};
        var bits=new int[values.Length];for(int i=0;i<values.Length;i++) bits[i]=BitConverter.SingleToInt32Bits(values[i]);
        return bits;
    }
    static void Vector(SerializedObject target, string field, Vector3 v)
    {
        var p=target.FindProperty(field);
        p.FindPropertyRelative("x").floatValue=v.x;p.FindPropertyRelative("y").floatValue=v.y;p.FindPropertyRelative("z").floatValue=v.z;
    }
    static void Assign(Transform transform,Node row)
    {
        using(var serialized=new SerializedObject(transform))
        {
            Vector(serialized,"m_LocalPosition",row.p);Vector(serialized,"m_LocalScale",row.s);
            var q=serialized.FindProperty("m_LocalRotation");
            q.FindPropertyRelative("x").floatValue=row.q.x;q.FindPropertyRelative("y").floatValue=row.q.y;
            q.FindPropertyRelative("z").floatValue=row.q.z;q.FindPropertyRelative("w").floatValue=row.q.w;
            serialized.ApplyModifiedPropertiesWithoutUndo();
        }
    }
    static string PrivateRoot(string variable)
    {
        var path=Path.GetFullPath(Environment.GetEnvironmentVariable(variable) ?? throw new Exception("Missing private input/output root"));
        for(var dir=new DirectoryInfo(path);dir!=null;dir=dir.Parent)
            if(Directory.Exists(Path.Combine(dir.FullName,".git")) || File.Exists(Path.Combine(dir.FullName,".git"))) throw new Exception("Private fixture must remain outside source control");
        return path;
    }
    public static void Run()
    {
        try
        {
            if(Application.unityVersion!="6000.0.64f1") throw new Exception("Unexpected Unity transform profile");
            string inputRoot=PrivateRoot("FOA_TRANSFORM_INPUT_ROOT"), outputRoot=PrivateRoot("FOA_TRANSFORM_OUTPUT_ROOT");
            Directory.CreateDirectory(outputRoot);
            var files=Directory.GetFiles(inputRoot,"*.unity-input.json");Array.Sort(files,StringComparer.Ordinal);
            if(files.Length<1 || files.Length>32) throw new Exception("Invalid transform input batch");
            foreach(var file in files)
            {
                if(new FileInfo(file).Length>32*1024*1024) throw new Exception("Input size exceeds bound");
                byte[] raw=File.ReadAllBytes(file);
                var input=JsonUtility.FromJson<Input>(System.Text.Encoding.UTF8.GetString(raw));
                if(input.nodes==null || input.nodes.Length<1 || input.nodes.Length>100000) throw new Exception("Invalid transform count");
                var output=new Output { unityVersion=Application.unityVersion,scope="Direct serialized local TRS evaluated by Unity Transform; signed-zero canonicalization recorded; runtime scripts and RectTransform layout are not executed" };
                using(var hash=SHA256.Create()) output.inputSha256=BitConverter.ToString(hash.ComputeHash(raw)).Replace("-","").ToLowerInvariant();
                var objects=new GameObject[input.nodes.Length];
                try
                {
                    for(int i=0;i<input.nodes.Length;i++)
                    {
                        var row=input.nodes[i];
                        if(row.parent < -1 || row.parent>=i || (row.kind!="Transform" && row.kind!="RectTransform")) throw new Exception("Invalid ordered source hierarchy");
                        if(row.kind=="RectTransform") output.rectTransforms++;
                        var go=objects[i]=new GameObject("Private source transform "+i) { hideFlags=HideFlags.HideAndDontSave };
                        if(row.parent>=0) go.transform.SetParent(objects[row.parent].transform,false);
                        Assign(go.transform,row);
                        output.rows.Add(new Row { localBits=MatrixBits(Matrix4x4.TRS(go.transform.localPosition,go.transform.localRotation,go.transform.localScale)),worldBits=MatrixBits(go.transform.localToWorldMatrix),inputBits=Bits(row.p,row.q,row.s),assignedBits=Bits(go.transform.localPosition,go.transform.localRotation,go.transform.localScale),p=go.transform.localPosition,q=go.transform.localRotation,s=go.transform.localScale,
                            local=Matrix(Matrix4x4.TRS(go.transform.localPosition,go.transform.localRotation,go.transform.localScale)),world=Matrix(go.transform.localToWorldMatrix) });
                    }
                    output.status="PASSED";
                    var destination=Path.Combine(outputRoot,Path.GetFileName(file)+".result.json");
                    if(File.Exists(destination)) throw new Exception("Refusing to replace earlier transform evidence");
                    File.WriteAllText(destination,JsonUtility.ToJson(output));
                    Debug.Log("FOA_TRANSFORM_CAPTURE "+Path.GetFileName(file)+" "+output.rows.Count);
                }
                finally
                {
                    for(int i=0;i<objects.Length;i++) if(objects[i]) UnityEngine.Object.DestroyImmediate(objects[i]);
                }
                GC.Collect();
            }
            EditorApplication.Exit(0);
        }
        catch(Exception error) { Debug.LogException(error); EditorApplication.Exit(1); }
    }
}
