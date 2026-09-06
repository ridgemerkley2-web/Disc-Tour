using UnrealBuildTool;
using System.Collections.Generic;

public class DiscGolfTourTarget : TargetRules
{
    private static readonly HashSet<string> ShippingIgnoredPluginDependencies = new()
    {
        "ConcertMain",
        "ConcertSyncClient",
        "ConcertSyncCore",
        "ConcertSharedSlate",
        "AssetManagerEditor",
    };

    public DiscGolfTourTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("DiscGolfTour");
    }

    public override bool ShouldIgnorePluginDependency(
        PluginInfo parentInfo,
        PluginReferenceDescriptor descriptor)
    {
        if (Configuration == UnrealTargetConfiguration.Shipping
            && ShippingIgnoredPluginDependencies.Contains(descriptor.Name))
        {
            return true;
        }

        return base.ShouldIgnorePluginDependency(parentInfo, descriptor);
    }
}
