using UnrealBuildTool;

public class KodEditor : ModuleRules
{
	public KodEditor(ReadOnlyTargetRules Target) : base(Target)
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
			"KodGenerals"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"UnrealEd",
			"AssetTools",
			"EditorSubsystem",
			"DataValidation"
		});
	}
}
