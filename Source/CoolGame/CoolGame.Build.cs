// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;
using System.IO;

public class CoolGame : ModuleRules
{
	public CoolGame(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "OnlineSubsystem", "OnlineSubsystemUtils", "OnlineSubsystemEOS" });

		PrivateDependencyModuleNames.AddRange(new string[] {  });

        // Lägg i konstruktorn för projektet
        string EOSPath = Path.Combine(ModuleDirectory, "../../ThirdParty/EOS-SDK-44532354-Release-v1.17.1.3");
        PublicIncludePaths.Add(Path.Combine(EOSPath, "SDK/Include"));
        PublicAdditionalLibraries.Add(Path.Combine(EOSPath, "SDK/Lib/EOSSDK-Win64-Shipping.lib"));

        // Uncomment if you are using Slate UI
        // PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

        // Uncomment if you are using online features
        // PrivateDependencyModuleNames.Add("OnlineSubsystem");

        // To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
    }
}