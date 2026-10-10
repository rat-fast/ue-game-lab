using UnrealBuildTool;

public class SkillLabSkills : ModuleRules
{
	public SkillLabSkills(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "GameplayAbilities", "GameplayTags", "GameplayTasks", "SkillLabGAS"
		});
		PrivateDependencyModuleNames.Add("Json");
	}
}
