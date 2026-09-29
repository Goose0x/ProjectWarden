using UnrealBuildTool;

public class KodUnits : ModuleRules
{
	public KodUnits(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"KodCore",
			"GameplayAbilities",
			"GameplayTags",
			"GameplayTasks",
			"NavigationSystem",
			"AIModule"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });
	}
}
