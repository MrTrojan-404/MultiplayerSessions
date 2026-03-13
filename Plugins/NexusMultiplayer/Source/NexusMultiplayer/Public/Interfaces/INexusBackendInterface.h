// Copyright NexusMultiplayer. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/NexusTypes.h"

class NEXUSMULTIPLAYER_API INexusBackendInterface
{
public:
	virtual ~INexusBackendInterface() = default;

	virtual void    Login() = 0;
	virtual bool    IsLoggedIn() const = 0;
	virtual ENexusBackendMode GetMode() const = 0;

	virtual FString GetDisplayName() const = 0;
	virtual FString GetBackendUserId() const = 0;
	virtual void    SetDisplayName(const FString& NewName) = 0;
	virtual void    FetchAccount() = 0;

	virtual void SubmitScore(const FString& LeaderboardId, int64 Score) = 0;
	virtual void FetchTopScores(const FString& LeaderboardId, int32 Limit) = 0;

	// Delegates — set by NexusMultiplayerSubsystem
	TFunction<void()>                               OnAccountLoaded;
	TFunction<void(FString)>                        OnDisplayNameUpdated;
	TFunction<void(FString)>                        OnLoginError;

	// Leaderboard delegates
	TFunction<void(FString /*LeaderboardId*/)>                          OnScoreSubmitted;
	TFunction<void(FString /*LeaderboardId*/, TArray<FNexusLeaderboardEntry>)> OnTopScoresFetched;
	TFunction<void(FString /*Error*/)>                                  OnLeaderboardError;
};