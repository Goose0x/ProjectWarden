using UnrealBuildTool;

public class KingdomOfDust : ModuleRules
{
	public KingdomOfDust(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"KodCore",
			"KodUnits",
			"KodEconomy",
			"KodGenerals",
			"KodAI",
			"KodNet",
			"KodUI"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });
	}
}
