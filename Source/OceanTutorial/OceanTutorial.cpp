// Fill out your copyright notice in the Description page of Project Settings.

#include "OceanTutorial.h"
#include "Modules/ModuleManager.h"

void FOceanTutorialModule::StartupModule()
{
	FString ShaderDirectory = FPaths::Combine(FPaths::ProjectDir(), TEXT("Shaders"));
	AddShaderSourceDirectoryMapping("/Project", ShaderDirectory);
}

IMPLEMENT_PRIMARY_GAME_MODULE( FOceanTutorialModule, OceanTutorial, "OceanTutorial" );
