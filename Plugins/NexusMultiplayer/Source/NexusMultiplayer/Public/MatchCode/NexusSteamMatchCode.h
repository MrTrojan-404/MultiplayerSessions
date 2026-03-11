// Copyright NexusMultiplayer. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Interfaces/INexusMatchCodeInterface.h"
#include "Interfaces/INexusOnlineInterface.h"

/**
 * FNexusSteamMatchCode
 *
 * Match code storage via Steam lobby metadata (session attributes).
 *
 * StoreCode  → writes "MatchCode" key into live Steam lobby via SetSessionAttribute
 * LookupCode → runs FindSessionByAttribute("MatchCode", Code)
 *              fires OnSessionFound(SessionId) on match, OnCodeError on miss
 * DeleteCode → clears the attribute (host cleanup on session end)
 *
 * Tradeoffs:
 *   + Instant — no server roundtrip
 *   + Works offline from Nakama
 *   - Dies when host leaves (lobby destroyed)
 *   - Requires an active Steam session
 */
class NEXUSMULTIPLAYER_API FNexusSteamMatchCode : public INexusMatchCodeInterface
{
public:
	explicit FNexusSteamMatchCode(TSharedPtr<INexusOnlineInterface> InOnline);

	virtual void StoreCode(const FString& Code, const FString& SessionId) override;
	virtual void LookupCode(const FString& Code) override;
	virtual void DeleteCode(const FString& Code) override;

private:
	TSharedPtr<INexusOnlineInterface> Online;

	static const FString MatchCodeKey;

	void HandleAttributeSessionFound(const FString& SessionId);
};