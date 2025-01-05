// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CustomWater : ModuleRules
{
    public CustomWater(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PrivateIncludePaths.AddRange(
            new string[] {
                System.IO.Path.Combine(GetModuleDirectory("Renderer"), "Private"),
            }
        );

        PublicDependencyModuleNames.AddRange(
            new string[] {
                "Core",
                "CoreUObject",
                "Engine",
                "RHI",
                "Niagara"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[] {
                "RenderCore",
                "Projects"
            }
        );

        bool bWithWaterSelectionSupport = false;
        if (Target.bBuildEditor)
        {
            bWithWaterSelectionSupport = true;

        }
        // Add a feature define instead of relying on the generic WITH_EDITOR define
        PublicDefinitions.Add("WITH_WATER_SELECTION_SUPPORT=" + (bWithWaterSelectionSupport ? 1 : 0));
    }
}