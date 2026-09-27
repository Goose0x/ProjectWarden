using UnrealBuildTool;

public class KodGenerals : ModuleRules
{
	public KodGenerals(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"KodCore",
			"KodUnits",
			"GameplayAbilities",
			"GameplayTags",
			"GameplayTasks"
		});
	}
}
