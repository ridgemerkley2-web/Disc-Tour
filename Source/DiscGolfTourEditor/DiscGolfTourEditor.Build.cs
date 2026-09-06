using UnrealBuildTool;

public class DiscGolfTourEditor : ModuleRules
{
    public DiscGolfTourEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        // The existing runtime module predates Public/Private folders and keeps its exported
        // headers at the module root. Expose that established layout only to this editor bridge.
        PublicIncludePaths.Add(System.IO.Path.Combine(ModuleDirectory, "../DiscGolfTour"));
        PrivateIncludePaths.Add(ModuleDirectory);

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "PhysicsCore",
            "DiscGolfTour",
            "DiscGolfRuntimeFoundation"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "AnimGraph",
            "AssetRegistry",
            "AssetTools",
            "BlueprintGraph",
            "ControlRig",
            "ControlRigDeveloper",
            "IKRig",
            "IKRigEditor",
            "Json",
            "JsonUtilities",
            "MetaHumanSDKRuntime",
            "PCG",
            "Projects",
            "RigVM",
            "RigVMDeveloper",
            "Slate",
            "SlateCore",
            "UMG",
            "UMGEditor",
            "UnrealEd"
        });
    }
}
