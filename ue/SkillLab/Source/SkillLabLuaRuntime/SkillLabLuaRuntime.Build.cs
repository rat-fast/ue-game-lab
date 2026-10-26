using UnrealBuildTool;

public class SkillLabLuaRuntime : ModuleRules
{
	public SkillLabLuaRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.Add("Core");
		// UBT 扫描并编译此模块目录下的 Lua C 源码；上游头仅供本模块使用。
		PrivateIncludePaths.Add(System.IO.Path.Combine(ModuleDirectory, "ThirdParty", "lua-5.4.8", "src"));
	}
}
