using UnrealBuildTool;

public class DiscGolfTour : ModuleRules
{
    public DiscGolfTour(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "RHI",
            "GameplayTags",
            "PhysicsCore",
            "InputCore",
            "EnhancedInput",
            "Json",
            "JsonUtilities",
            "ProceduralMeshComponent",
            "PCG",
            "UMG",
            "Slate",
            "SlateCore"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Projects"
        });

        // Regression presets are intentionally human-readable non-asset data.
        // Stage them beside the packaged project so FPaths::ProjectDir keeps the
        // same Data/PhysicsRegressionPresets.json contract as editor builds.
        RuntimeDependencies.Add(
            System.IO.Path.Combine(ModuleDirectory, "../../Data/PhysicsRegressionPresets.json"),
            StagedFileType.NonUFS);
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
