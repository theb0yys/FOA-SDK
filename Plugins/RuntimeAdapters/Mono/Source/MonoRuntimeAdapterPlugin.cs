using BepInEx;

namespace FoaSdk.RuntimeAdapters.Mono;

[BepInPlugin(PluginGuid, PluginName, PluginVersion)]
[BepInDependency(FrameworkGuid, FrameworkVersion)]
public sealed class MonoRuntimeAdapterPlugin : BaseUnityPlugin
{
    public const string PluginGuid = "foa.sdk.runtime-adapter.mono";
    public const string PluginName = "FOA SDK Mono Runtime Adapter";
    public const string PluginVersion = "0.1.0";
    public const string FrameworkGuid = "kane.tgfoa.tainted-framework";
    public const string FrameworkVersion = "0.1.33";
    public const string StartupMarker = "foa-sdk.mono-adapter.ready";

    private void Awake()
    {
        Logger.LogInfo(
            $"{StartupMarker}; adapter=adapter.foa.mono; package={PluginVersion}; " +
            "game=1.23.401; branch=mono; runtime=Mono; unity=6000.0.64f1; " +
            "loader=BepInEx-5.4.23.3; framework=0.1.33; " +
            "gameApi=false; mutation=false; saveAccess=false.");
    }
}
