// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "PythonProject.h"
#include "IPythonScriptPlugin.h"


UPythonProject::UPythonProject(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	FString ProjectScriptsPath = FPaths::Combine(*FPaths::ProjectContentDir(), UTF8_TO_TCHAR("Scripts"));
	Path = ProjectScriptsPath;
	//FUnrealEnginePythonModule &PythonModule = FModuleManager::GetModuleChecked<FUnrealEnginePythonModule>("UnrealEnginePython");
	//Path = PythonModule.ScriptsPaths[0];
}
