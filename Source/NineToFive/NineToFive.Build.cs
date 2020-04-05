// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;

public class NineToFive : ModuleRules
{
	public NineToFive(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore" , "HeadMountedDisplay", "NavigationSystem", "AIModule",
            "UMG", "Slate", "SlateCore", "RenderCore", "Paper2D"});

		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore", "RenderCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
