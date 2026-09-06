// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AINativeRoguelite : ModuleRules
{
	public AINativeRoguelite(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"NavigationSystem",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"Niagara",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"AINativeRoguelite",
			"AINativeRoguelite/Variant_Strategy",
			"AINativeRoguelite/Variant_Strategy/UI",
			"AINativeRoguelite/Variant_TwinStick",
			"AINativeRoguelite/Variant_TwinStick/AI",
			"AINativeRoguelite/Variant_TwinStick/Gameplay",
			"AINativeRoguelite/Variant_TwinStick/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
