// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class NetworkedPhysicsTarget : TargetRules
{
	public NetworkedPhysicsTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V6;
		ExtraModuleNames.Add("NetworkedPhysics");
	}
}
