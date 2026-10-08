// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class NetworkedPhysicsEditorTarget : TargetRules
{
	public NetworkedPhysicsEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		ExtraModuleNames.Add("NetworkedPhysics");
	}
}
