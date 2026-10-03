// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class WEVORA : ModuleRules
{
	public WEVORA(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"WEVORA",
			"WEVORA/Variant_Platforming",
			"WEVORA/Variant_Platforming/Animation",
			"WEVORA/Variant_Combat",
			"WEVORA/Variant_Combat/AI",
			"WEVORA/Variant_Combat/Animation",
			"WEVORA/Variant_Combat/Gameplay",
			"WEVORA/Variant_Combat/Interfaces",
			"WEVORA/Variant_Combat/UI",
			"WEVORA/Variant_SideScrolling",
			"WEVORA/Variant_SideScrolling/AI",
			"WEVORA/Variant_SideScrolling/Gameplay",
			"WEVORA/Variant_SideScrolling/Interfaces",
			"WEVORA/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
