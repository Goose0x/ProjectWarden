// Kingdom of Dust — clean-room Unreal RTS
using UnrealBuildTool;
using System.Collections.Generic;

public class KingdomOfDustTarget : TargetRules
{
	public KingdomOfDustTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
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
			"KodUI"
		});
	}
}
