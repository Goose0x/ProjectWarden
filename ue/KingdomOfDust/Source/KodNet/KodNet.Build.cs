using UnrealBuildTool;

public class KodNet : ModuleRules
{
	public KodNet(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"KodCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"KodUnits",
			"KodEconomy",
			"KodGenerals"
		});
	}
}
