using UnrealBuildTool;

public class DiscGolfCharacterFramework : ModuleRules
{
    public DiscGolfCharacterFramework(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "AnimGraphRuntime"
        });
    }
}
