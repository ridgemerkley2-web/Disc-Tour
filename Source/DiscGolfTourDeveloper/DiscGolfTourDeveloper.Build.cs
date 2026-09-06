using UnrealBuildTool;

public class DiscGolfTourDeveloper : ModuleRules
{
    public DiscGolfTourDeveloper(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDefinitions.Add("DG_DEVELOPER_WITH_THROW_LAB=1");
        PublicIncludePaths.Add(System.IO.Path.Combine(ModuleDirectory, "../DiscGolfTour"));
        PrivateIncludePaths.Add(ModuleDirectory);

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "DiscGolfTour",
            "DiscGolfRuntimeFoundation"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "AnimationCore",
            "ApplicationCore",
            "ControlRig",
            "GameplayTags",
            "HairStrandsCore",
            "IKRig",
            "InputCore",
            "Json",
            "JsonUtilities",
            "Projects",
            "RHI",
            "RigVM",
            "Slate",
            "SlateCore",
            "UMG"
        });
    }
}
