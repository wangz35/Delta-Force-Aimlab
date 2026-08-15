using UnrealBuildTool;
using System.Collections.Generic;

public class AimTrackerEditorTarget : TargetRules
{
    public AimTrackerEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.AddRange(new string[] { "AimTracker" });
    }
}
