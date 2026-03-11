// Copyright NexusMultiplayer. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

DECLARE_LOG_CATEGORY_EXTERN(LogNexus, Log, All);

// Works in Editor AND packaged Development/DebugGame builds.
// Stripped only in Shipping. Use these to track bugs in packaged builds.

// Flow message — 10 seconds, white. Use for normal step-by-step tracking.
#if !UE_BUILD_SHIPPING
#define NEXUS_LOG(Format, ...) \
UE_LOG(LogNexus, Log, TEXT(Format), ##__VA_ARGS__); \
if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::White, \
FString::Printf(TEXT("[Nexus] " Format), ##__VA_ARGS__))

// Success message — 10 seconds, green.
#define NEXUS_SUCCESS(Format, ...) \
UE_LOG(LogNexus, Log, TEXT(Format), ##__VA_ARGS__); \
if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Green, \
FString::Printf(TEXT("[Nexus] " Format), ##__VA_ARGS__))

// Warning — 15 seconds, yellow. Use for unexpected but recoverable states.
#define NEXUS_WARN(Format, ...) \
UE_LOG(LogNexus, Warning, TEXT(Format), ##__VA_ARGS__); \
if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::Yellow, \
FString::Printf(TEXT("[Nexus] " Format), ##__VA_ARGS__))

// Error — 20 seconds, red. Use for failures.
#define NEXUS_ERROR(Format, ...) \
UE_LOG(LogNexus, Error, TEXT(Format), ##__VA_ARGS__); \
if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 20.f, FColor::Red, \
FString::Printf(TEXT("[Nexus] " Format), ##__VA_ARGS__))
#else
// Shipping — log to file only, no screen spam
#define NEXUS_LOG(Format, ...)     UE_LOG(LogNexus, Log,     TEXT(Format), ##__VA_ARGS__)
#define NEXUS_SUCCESS(Format, ...) UE_LOG(LogNexus, Log,     TEXT(Format), ##__VA_ARGS__)
#define NEXUS_WARN(Format, ...)    UE_LOG(LogNexus, Warning, TEXT(Format), ##__VA_ARGS__)
#define NEXUS_ERROR(Format, ...)   UE_LOG(LogNexus, Error,   TEXT(Format), ##__VA_ARGS__)
#endif