// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
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
			ModuleDirectory,
			Path.Combine(ModuleDirectory, "Variant_Platforming"),
			Path.Combine(ModuleDirectory, "Variant_Platforming", "Animation"),
			Path.Combine(ModuleDirectory, "Variant_Combat"),
			Path.Combine(ModuleDirectory, "Variant_Combat", "AI"),
			Path.Combine(ModuleDirectory, "Variant_Combat", "Animation"),
			Path.Combine(ModuleDirectory, "Variant_Combat", "Gameplay"),
			Path.Combine(ModuleDirectory, "Variant_Combat", "Interfaces"),
			Path.Combine(ModuleDirectory, "Variant_Combat", "UI"),
			Path.Combine(ModuleDirectory, "Variant_SideScrolling"),
			Path.Combine(ModuleDirectory, "Variant_SideScrolling", "AI"),
			Path.Combine(ModuleDirectory, "Variant_SideScrolling", "Gameplay"),
			Path.Combine(ModuleDirectory, "Variant_SideScrolling", "Interfaces"),
			Path.Combine(ModuleDirectory, "Variant_SideScrolling", "UI")
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
