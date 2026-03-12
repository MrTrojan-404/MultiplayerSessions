// Copyright Epic Games, Inc. All Rights Reserved.

#include "NexusMultiplayer.h"

#include "Core/NexusLog.h"

DEFINE_LOG_CATEGORY(LogNexus);

#define LOCTEXT_NAMESPACE "FNexusMultiplayerModule"

void FNexusMultiplayerModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	NEXUS_LOG("[NexusMultiplayer] Plugin started");
}

void FNexusMultiplayerModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
	
	NEXUS_LOG("[NexusMultiplayer] Plugin shutdown");

}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FNexusMultiplayerModule, NexusMultiplayer)

