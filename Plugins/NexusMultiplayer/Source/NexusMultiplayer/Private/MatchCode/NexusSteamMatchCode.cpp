// Copyright NexusMultiplayer. All Rights Reserved.

#include "MatchCode/NexusSteamMatchCode.h"
#include "Core/NexusLog.h"

const FString FNexusSteamMatchCode::MatchCodeKey = TEXT("NexusMatchCode");

FNexusSteamMatchCode::FNexusSteamMatchCode(
    TSharedPtr<INexusOnlineInterface> InOnline)
    : Online(InOnline)
{
    NEXUS_LOG("[SteamMatchCode] Created. Online valid: %s",
        Online.IsValid() ? TEXT("YES") : TEXT("NO"));

    // Wire up the online interface callback so we hear about attribute searches
    if (Online.IsValid())
    {
        Online->OnAttributeSessionFound = [this](const FString& SessionId)
        {
            HandleAttributeSessionFound(SessionId);
        };
    }
}

// ── StoreCode ─────────────────────────────────────────────────────────────────

void FNexusSteamMatchCode::StoreCode(
    const FString& Code, const FString& SessionId)
{
    // SessionId is implicit in Steam (we're the host, lobby is ours)
    // — we store it locally for confirmation logging but don't need it
    // because SetSessionAttribute always writes to the active session.

    if (!Online.IsValid())
    {
        NEXUS_ERROR("[SteamMatchCode] StoreCode: Online interface null");
        if (OnCodeError) OnCodeError(TEXT("Online interface not available"));
        return;
    }

    NEXUS_LOG("[SteamMatchCode] StoreCode: writing key '%s' = '%s' to lobby",
        *MatchCodeKey, *Code);

    Online->SetSessionAttribute(MatchCodeKey, Code);

    NEXUS_SUCCESS("[SteamMatchCode] Match code '%s' stored in Steam lobby metadata", *Code);

    if (OnCodeStored) OnCodeStored(Code);
}

// ── LookupCode ────────────────────────────────────────────────────────────────

void FNexusSteamMatchCode::LookupCode(const FString& Code)
{
    if (!Online.IsValid())
    {
        NEXUS_ERROR("[SteamMatchCode] LookupCode: Online interface null");
        if (OnCodeError) OnCodeError(TEXT("Online interface not available"));
        return;
    }

    if (Code.IsEmpty())
    {
        NEXUS_ERROR("[SteamMatchCode] LookupCode: code is empty");
        if (OnCodeError) OnCodeError(TEXT("Match code is empty"));
        return;
    }

    NEXUS_LOG("[SteamMatchCode] LookupCode: searching for sessions with '%s' = '%s'",
        *MatchCodeKey, *Code);

    Online->FindSessionByAttribute(MatchCodeKey, Code);
    // Result comes back via Online->OnAttributeSessionFound → HandleAttributeSessionFound
}

// ── DeleteCode ────────────────────────────────────────────────────────────────

void FNexusSteamMatchCode::DeleteCode(const FString& Code)
{
    if (!Online.IsValid())
    {
        NEXUS_WARN("[SteamMatchCode] DeleteCode: Online interface null — nothing to clear");
        return;
    }

    NEXUS_LOG("[SteamMatchCode] DeleteCode: clearing '%s' from lobby metadata", *Code);

    // Clear the attribute so the lobby no longer shows up in code searches
    Online->SetSessionAttribute(MatchCodeKey, TEXT(""));

    NEXUS_LOG("[SteamMatchCode] Match code cleared from Steam lobby");
}

// ── Callback ──────────────────────────────────────────────────────────────────

void FNexusSteamMatchCode::HandleAttributeSessionFound(const FString& SessionId)
{
    if (SessionId.IsEmpty())
    {
        NEXUS_WARN("[SteamMatchCode] Lookup result: no session found for that code");
        if (OnCodeError) OnCodeError(TEXT("No session found with that match code"));
        return;
    }

    NEXUS_SUCCESS("[SteamMatchCode] Lookup result: found session ID '%s'", *SessionId);
    if (OnSessionFound) OnSessionFound(SessionId);
}