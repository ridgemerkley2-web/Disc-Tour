using UnrealBuildTool;

public class DiscGolfTour : ModuleRules
{
    public DiscGolfTour(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        bool bReleaseShipping = Target.Configuration == UnrealTargetConfiguration.Shipping;
        PublicDefinitions.Add("DG_RELEASE_V05_SCOPE=" + (bReleaseShipping ? "1" : "0"));
        PublicDefinitions.Add("DG_WITH_CAREER_AI=" + (bReleaseShipping ? "0" : "1"));
        PublicDefinitions.Add("DG_WITH_THROW_LAB=0");
        PublicDefinitions.Add("DG_WITH_DEVELOPMENT_CONTENT=" + (bReleaseShipping ? "0" : "1"));
        // The Shipping SKU retains one fail-closed, unattended performance gate.
        // It does not enable developer content, console commands, test runners,
        // regression presets, or gameplay cheats.
        PublicDefinitions.Add("DG_WITH_RELEASE_PERFORMANCE_CAPTURE=" + (bReleaseShipping ? "1" : "0"));

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "AnimationCore",
            "ControlRig",
            "Engine",
            "RHI",
            "RigVM",
            "GameplayTags",
            "PhysicsCore",
            "InputCore",
            "EnhancedInput",
            "Json",
            "JsonUtilities",
            "IKRig",
            "MetaHumanSDKRuntime",
            "HairStrandsCore",
            "ProceduralMeshComponent",
            "UMG",
            "Slate",
            "SlateCore"
        });

        PublicDependencyModuleNames.Add("DiscGolfRuntimeFoundation");

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Projects"
        });

        // Regression presets are development-only evidence and must never be
        // staged into the v0.5 Shipping SKU.
        if (!bReleaseShipping)
        {
            RuntimeDependencies.Add(
                System.IO.Path.Combine(ModuleDirectory, "../../Data/PhysicsRegressionPresets.json"),
                StagedFileType.NonUFS);
        }
        RuntimeDependencies.Add(
            System.IO.Path.Combine(ModuleDirectory, "../../Data/PineRidgeHole1.json"),
            StagedFileType.NonUFS);
        RuntimeDependencies.Add(
            System.IO.Path.Combine(ModuleDirectory, "../../Data/PineRidgeHole2.json"),
            StagedFileType.NonUFS);
        RuntimeDependencies.Add(
            System.IO.Path.Combine(ModuleDirectory, "../../Data/PineRidgeHole3.json"),
            StagedFileType.NonUFS);
        RuntimeDependencies.Add(
            System.IO.Path.Combine(ModuleDirectory, "../../Data/PineRidgeCourse.json"),
            StagedFileType.NonUFS);
        RuntimeDependencies.Add(
            System.IO.Path.Combine(ModuleDirectory, "../../Data/PineRidgePresentation.json"),
            StagedFileType.NonUFS);
    }
}
