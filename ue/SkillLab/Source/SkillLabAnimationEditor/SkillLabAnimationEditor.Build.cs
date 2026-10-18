using UnrealBuildTool;

public class SkillLabAnimationEditor : ModuleRules
{
	public SkillLabAnimationEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "Json", "JsonUtilities", "AssetRegistry", "GameplayTags"
		});
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"UnrealEd", "SkillLabSkills"
		});
	}
}
