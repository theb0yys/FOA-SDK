// SPDX-License-Identifier: Apache-2.0 OR MIT
using BepInEx;
using BepInEx.Unity.IL2CPP;

namespace FOA.SDK.RuntimeAdapter.IL2CPP;

[BepInPlugin(PluginGuid, PluginName, PluginVersion)]
[BepInDependency(FrameworkGuid, FrameworkVersion)]
public sealed class Plugin : BasePlugin
{
    public const string PluginGuid = "foa.sdk.runtime-adapter.il2cpp";
    public const string PluginName = "FOA SDK IL2CPP Runtime Adapter";
    public const string PluginVersion = "0.1.0";
    public const string FrameworkGuid = "kane.tgfoa.tainted-framework";
    public const string FrameworkVersion = "0.1.36";
    public const string StartupMarker = "foa-sdk.il2cpp-adapter.ready";

    public override void Load()
    {
        Log.LogInfo(
            $"{StartupMarker}; version={PluginVersion}; runtime=IL2CPP; " +
            "reportOnly=true; mutationAllowed=false; saveAccessAllowed=false");
    }
}
