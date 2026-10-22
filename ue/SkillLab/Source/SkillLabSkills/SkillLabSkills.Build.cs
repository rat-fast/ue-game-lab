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
		string LuaSourceDir = System.IO.Path.Combine(ModuleDirectory, "ThirdParty", "lua-5.4.8", "src");
		PrivateIncludePaths.Add(LuaSourceDir);
		// Lua C sources live below this module directory and are picked up by UBT's source scanner.
	}
}
