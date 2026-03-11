// Copyright NexusMultiplayer. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "NexusNakamaSession.generated.h"

UCLASS()
class NEXUSMULTIPLAYER_API UNexusNakamaSession : public USaveGame
{
	GENERATED_BODY()
public:
	UPROPERTY() FString AuthToken;
	UPROPERTY() FString RefreshToken;

	
};