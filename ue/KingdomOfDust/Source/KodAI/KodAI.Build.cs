using UnrealBuildTool;

public class KodAI : ModuleRules
{
	public KodAI(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"KodCore",
			"KodUnits",
			"KodNet",
			"AIModule",
			"GameplayTasks",
			"NavigationSystem"
		});

		// StateTreeModule — enable when team adopts StateTree for commander flow (M2+)
	}
}
