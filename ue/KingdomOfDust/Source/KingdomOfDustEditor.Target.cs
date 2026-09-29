// Kingdom of Dust — clean-room Unreal RTS
using UnrealBuildTool;
using System.Collections.Generic;

public class KingdomOfDustEditorTarget : TargetRules
{
	public KingdomOfDustEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_4;
		ExtraModuleNames.AddRange(new string[]
		{
			"KingdomOfDust",
			"KodCore",
			"KodUnits",
			"KodEconomy",
			"KodGenerals",
			"KodAI",
			"KodNet",
			"KodUI",
			"KodEditor"
		});
	}
}
