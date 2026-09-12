// Copyright (c) Contributors to the Open 3D Engine Project.
// For complete copyright and license terms please see the LICENSE at the root of this distribution.
// SPDX-License-Identifier: Apache-2.0 OR MIT
// Synthetic settings only. No game files are passed to Unity or changed.
using System;
using System.Collections.Generic;
using System.IO;
using UnityEditor;
using UnityEditor.Build.Content;
using UnityEngine;

public static class FoaGameSettingsProbe
{
    [Serializable] public class Case
    {
        public string colorSpace;
        public int colorSpaceValue;
        public string[] files;
    }
    [Serializable] public class Report
    {
        public string status = "FAILED", unityVersion, error;
        public List<Case> cases = new List<Case>();
    }
    static string PrivateDirectory(string path)
    {
        if (String.IsNullOrEmpty(path) || !Path.IsPathRooted(path) || !Directory.Exists(path))
            throw new InvalidOperationException("An existing private directory is required.");
        string full = Path.GetFullPath(path);
        for (var parent = new DirectoryInfo(full); parent != null; parent = parent.Parent)
            if (Directory.Exists(Path.Combine(parent.FullName,".git")) || File.Exists(Path.Combine(parent.FullName,".git")))
                throw new InvalidOperationException("Settings probes must remain outside a source checkout.");
        return full;
    }
    public static void Run()
    {
        string output = null;
        bool mayWriteReport = false;
        var report = new Report { unityVersion = Application.unityVersion };
        ColorSpace previous = PlayerSettings.colorSpace;
        try
        {
            PrivateDirectory(Path.GetDirectoryName(Application.dataPath));
            output = PrivateDirectory(Environment.GetEnvironmentVariable("FOA_SETTINGS_OUTPUT"));
            if (Application.unityVersion != "6000.0.64f1") throw new InvalidOperationException("Exact source-profile editor version required.");
            if (File.Exists(Path.Combine(output,"settings-probe.json"))) throw new InvalidOperationException("Probe output already exists.");
            mayWriteReport = true;
            foreach (ColorSpace space in new[] { ColorSpace.Gamma, ColorSpace.Linear })
            {
                string folder = Path.Combine(output,space.ToString());
                if (Directory.Exists(folder)) throw new InvalidOperationException("Settings fixture directory already exists.");
                Directory.CreateDirectory(folder);
                PlayerSettings.colorSpace = space;
                if (PlayerSettings.colorSpace != space) throw new InvalidOperationException("Synthetic setting was not applied.");
                var settings = new UnityEditor.Build.Content.BuildSettings
                {
                    target = BuildTarget.StandaloneWindows64,
                    group = BuildTargetGroup.Standalone,
                    buildFlags = ContentBuildFlags.None
                };
                using (var map = new BuildReferenceMap())
                {
                    var parameters = new WriteManagerParameters
                    {
                        settings = settings,
                        globalUsage = ContentBuildInterface.GetGlobalUsageFromGraphicsSettings(),
                        referenceMap = map
                    };
                    ContentBuildInterface.WriteGameManagersSerializedFile(folder,parameters);
                }
                string[] files = Directory.GetFiles(folder);
                if (files.Length == 0) throw new InvalidOperationException("Native settings writer produced no files.");
                report.cases.Add(new Case { colorSpace = space.ToString(), colorSpaceValue = (int)space, files = files });
            }
            report.status = "PASSED";
        }
        catch (Exception error) { report.error = error.ToString(); Debug.LogError(report.error); }
        finally
        {
            PlayerSettings.colorSpace = previous;
            if (mayWriteReport)
            {
                using (var file = new FileStream(Path.Combine(output,"settings-probe.json"),FileMode.CreateNew,FileAccess.Write))
                using (var writer = new StreamWriter(file)) writer.Write(JsonUtility.ToJson(report,true));
            }
            EditorApplication.Exit(report.status == "PASSED" ? 0 : 1);
        }
    }
}
