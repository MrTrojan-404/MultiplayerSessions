// Copyright NexusMultiplayer. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Interfaces/INexusBackendInterface.h"
#include "Interfaces/INexusOnlineInterface.h"

/**
 * FNexusPlatformBackend
 *
 * Backend implementation for PlatformOnly mode.
 * No external server required — delegates everything to INexusOnlineInterface.
 *
 * Display name  → Steam/EOS nickname
 * Auth          → always "logged in" (platform handles it)
 * Leaderboards  → platform native (Steam leaderboards)
 * Match codes   → handled by INexusMatchCodeInterface, not here
 */
class NEXUSMULTIPLAYER_API FNexusPlatformBackend : public INexusBackendInterface
{
public:
	explicit FNexusPlatformBackend(TSharedPtr<INexusOnlineInterface> InOnline);

	// INexusBackendInterface
	virtual void    Login() override;
	virtual bool    IsLoggedIn() const override;
	virtual ENexusBackendMode GetMode() const override { return ENexusBackendMode::PlatformOnly; }
	virtual FString GetDisplayName() const override;
	virtual FString GetBackendUserId() const override;
	virtual void    SetDisplayName(const FString& NewName) override;
	virtual void    FetchAccount() override;
	virtual void    SubmitScore(const FString& LeaderboardId, int64 Score) override;
	virtual void    FetchTopScores(const FString& LeaderboardId, int32 Limit) override;

private:
	TSharedPtr<INexusOnlineInterface> Online;
};