using UnrealBuildTool;
using System.Collections.Generic;

public class DiscGolfTourEditorTarget : TargetRules
{
    public DiscGolfTourEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.AddRange(new string[] { "DiscGolfTour", "DiscGolfTourEditor" });
    }
}
