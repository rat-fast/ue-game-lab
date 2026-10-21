using UnrealBuildTool;

public class SkillLabPresentation : ModuleRules
{
	public SkillLabPresentation(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "GameplayTags", "SkillLabSkills"
		});
		PrivateDependencyModuleNames.AddRange(new string[] { "AnimGraphRuntime", "Json" });
		PrivateIncludePaths.Add(System.IO.Path.Combine(ModuleDirectory, "ThirdParty", "lua-5.4.8", "src"));
	}
}
