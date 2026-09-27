using UnrealBuildTool;

public class KodEconomy : ModuleRules
{
	public KodEconomy(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"KodCore",
			"KodUnits",
			"GameplayTags"
		});
	}
}
