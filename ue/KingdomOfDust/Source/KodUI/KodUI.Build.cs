using UnrealBuildTool;

public class KodUI : ModuleRules
{
	public KodUI(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"KodCore",
			"KodUnits",
			"KodEconomy",
			"KodGenerals",
			"KodNet",
			"UMG",
			"Slate",
			"SlateCore",
			"CommonUI",
			"EnhancedInput",
			"InputCore",
			"GameplayTags"
		});
	}
}
