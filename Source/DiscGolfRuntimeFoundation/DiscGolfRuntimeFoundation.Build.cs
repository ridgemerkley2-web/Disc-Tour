using UnrealBuildTool;

public class DiscGolfRuntimeFoundation : ModuleRules
{
    public DiscGolfRuntimeFoundation(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine"
        });
    }
}
