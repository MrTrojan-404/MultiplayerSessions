// Copyright NexusMultiplayer. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Core/NexusTypes.h"

class FOnlineSessionSearchResult;

class NEXUSMULTIPLAYER_API INexusOnlineInterface
{
public:
	virtual ~INexusOnlineInterface() = default;

	// Identity
	virtual FString GetDisplayName() const = 0;
	virtual FString GetUserId() const = 0;
	virtual FString GetAuthTicket() const = 0;
	virtual bool IsAvailable() const = 0;
	virtual ENexusOnlineMode GetMode() const = 0;

	// Sessions
	virtual void CreateSession(const FNexusSessionConfig& Config) = 0;
	virtual void FindSessions(int32 MaxResults) = 0;
	virtual void FindSessionByAttribute(const FString& Key, const FString& Value) = 0;
	virtual void JoinSession(const FOnlineSessionSearchResult& Result) = 0;
	virtual void JoinSessionById(const FString& SessionId) = 0;
	virtual void DestroySession() = 0;
	virtual void SetSessionAttribute(const FString& Key, const FString& Value) = 0;

	// Delegates — set by NexusMultiplayerSubsystem
	TFunction<void(const TArray<FOnlineSessionSearchResult>&, bool)> OnFindComplete;
	TFunction<void(bool)>    OnCreateComplete;
	TFunction<void(bool)>    OnJoinComplete;
	TFunction<void(bool)>    OnDestroyComplete;
	TFunction<void(FString)> OnAttributeSessionFound;
};