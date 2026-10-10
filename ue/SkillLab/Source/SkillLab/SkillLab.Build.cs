// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SkillLab : ModuleRules
{
	public SkillLab(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		// Keep only modules required by public SkillLab headers here. Gameplay, navigation,
		// feedback and skill implementation details are private to this module.
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "InputCore", "UMG", "GameplayAbilities"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"EnhancedInput", "AIModule", "NavigationSystem", "Niagara", "Slate", "SlateCore",
			"GameplayTags", "GameplayTasks", "SkillLabGAS", "SkillLabSkills"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
