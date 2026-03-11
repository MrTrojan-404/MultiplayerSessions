// Copyright NexusMultiplayer. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

DECLARE_LOG_CATEGORY_EXTERN(LogNexus, Log, All);

#if !UE_BUILD_SHIPPING

#define NEXUS_LOG(Format, ...) \
UE_LOG(LogNexus, Log, TEXT(Format), ##__VA_ARGS__); \
if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::White, \
FString::Printf(TEXT("[Nexus] " Format), ##__VA_ARGS__))

#define NEXUS_SUCCESS(Format, ...) \
UE_LOG(LogNexus, Log, TEXT(Format), ##__VA_ARGS__); \
if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Green, \
FString::Printf(TEXT("[Nexus] " Format), ##__VA_ARGS__))

#define NEXUS_WARN(Format, ...) \
UE_LOG(LogNexus, Warning, TEXT(Format), ##__VA_ARGS__); \
if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::Yellow, \
FString::Printf(TEXT("[Nexus] " Format), ##__VA_ARGS__))

#define NEXUS_ERROR(Format, ...) \
UE_LOG(LogNexus, Error, TEXT(Format), ##__VA_ARGS__); \
if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 20.f, FColor::Red, \
FString::Printf(TEXT("[Nexus] " Format), ##__VA_ARGS__))

#else

#define NEXUS_LOG(Format, ...)     UE_LOG(LogNexus, Log,     TEXT(Format), ##__VA_ARGS__)
#define NEXUS_SUCCESS(Format, ...) UE_LOG(LogNexus, Log,     TEXT(Format), ##__VA_ARGS__)
#define NEXUS_WARN(Format, ...)    UE_LOG(LogNexus, Warning, TEXT(Format), ##__VA_ARGS__)
#define NEXUS_ERROR(Format, ...)   UE_LOG(LogNexus, Error,   TEXT(Format), ##__VA_ARGS__)

#endif