// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class parkour_GP4 : ModuleRules
{
	public parkour_GP4(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput" });
	}
}
