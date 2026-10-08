// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class NetworkedPhysics : ModuleRules
{
	public NetworkedPhysics(ReadOnlyTargetRules Target) : base(Target)
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
			"Slate",
			"Chaos",
			"ChaosVehicles",
			"ChaosVehiclesCore",
			"ChaosVehiclesEngine",
			"GeometryCollectionEngine",
			"ChaosModularVehicleEngine"
		});
		
		PublicIncludePaths.AddRange(new string[] {
			"NetworkedPhysics",
			"NetworkedPhysics/Variant_Platforming",
			"NetworkedPhysics/Variant_Platforming/Animation",
			"NetworkedPhysics/Variant_Combat",
			"NetworkedPhysics/Variant_Combat/AI",
			"NetworkedPhysics/Variant_Combat/Animation",
			"NetworkedPhysics/Variant_Combat/Gameplay",
			"NetworkedPhysics/Variant_Combat/Interfaces",
			"NetworkedPhysics/Variant_Combat/UI",
			"NetworkedPhysics/Variant_SideScrolling",
			"NetworkedPhysics/Variant_SideScrolling/AI",
			"NetworkedPhysics/Variant_SideScrolling/Gameplay",
			"NetworkedPhysics/Variant_SideScrolling/Interfaces",
			"NetworkedPhysics/Variant_SideScrolling/UI"
		});

		SetupIrisSupport(Target);
		
		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
