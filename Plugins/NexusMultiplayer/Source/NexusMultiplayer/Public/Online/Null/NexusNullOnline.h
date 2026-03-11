// Copyright NexusMultiplayer. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Interfaces/INexusOnlineInterface.h"
#include "Core/NexusLog.h"

/**
 * FNexusNullOnline
 *
 * Stub online layer for LAN / dev / PIE testing.
 * Uses the NULL OnlineSubsystem — sessions still work over LAN.
 * No auth tickets, no platform friends, no real matchmaking.
 *
 * All session ops fire their callbacks with failure so the subsystem
 * never hangs waiting. Use FNexusSteamOnline for real sessions.
 */
class NEXUSMULTIPLAYER_API FNexusNullOnline : public INexusOnlineInterface
{
public:

    FNexusNullOnline()
    {
        NEXUS_LOG("[NullOnline] Created — running in LAN/Dev mode. No platform features available.");
    }

    // ── Identity ──────────────────────────────────────────────────────────────

    virtual FString GetDisplayName() const override
    {
        return FPlatformProcess::UserName(false);
    }

    virtual FString GetUserId() const override
    {
        // Stable machine-unique string — good enough for LAN session IDs
        return FPlatformMisc::GetLoginId();
    }

    virtual FString GetAuthTicket() const override
    {
        NEXUS_WARN("[NullOnline] GetAuthTicket: no auth ticket in Null mode — backend auth will fail");
        return FString();
    }

    virtual bool IsAvailable() const override { return true; }

    virtual ENexusOnlineMode GetMode() const override
    {
        return ENexusOnlineMode::Null;
    }

    // ── Sessions ──────────────────────────────────────────────────────────────

    virtual void CreateSession(const FNexusSessionConfig& Config) override
    {
        // Null mode does not create real sessions.
        // If you want LAN sessions, wire up the NULL OSS properly in DefaultEngine.ini
        // and promote this to an actual session call — for now, just fail gracefully.
        NEXUS_WARN("[NullOnline] CreateSession called in Null mode — no session will be created. "
                   "Switch to Steam or EOS online mode for real sessions.");
        if (OnCreateComplete) OnCreateComplete(false);
    }

    virtual void FindSessions(int32 MaxResults) override
    {
        NEXUS_WARN("[NullOnline] FindSessions called in Null mode — returning empty results");
        if (OnFindComplete) OnFindComplete({}, false);
    }

    virtual void FindSessionByAttribute(
        const FString& Key, const FString& Value) override
    {
        NEXUS_WARN("[NullOnline] FindSessionByAttribute called in Null mode — Key='%s' Value='%s'. "
                   "No session will be found.", *Key, *Value);
        if (OnAttributeSessionFound) OnAttributeSessionFound(FString());
    }

    virtual void JoinSession(
        const FOnlineSessionSearchResult& Result) override
    {
        NEXUS_WARN("[NullOnline] JoinSession called in Null mode — join will fail");
        if (OnJoinComplete) OnJoinComplete(false);
    }

    virtual void JoinSessionById(const FString& SessionId) override
    {
        NEXUS_WARN("[NullOnline] JoinSessionById called in Null mode — SessionId='%s'. "
                   "Join will fail.", *SessionId);
        if (OnJoinComplete) OnJoinComplete(false);
    }

    virtual void DestroySession() override
    {
        NEXUS_LOG("[NullOnline] DestroySession called in Null mode — no-op, firing success");
        if (OnDestroyComplete) OnDestroyComplete(true);
    }

    virtual void SetSessionAttribute(
        const FString& Key, const FString& Value) override
    {
        NEXUS_WARN("[NullOnline] SetSessionAttribute: Key='%s' Value='%s' — "
                   "ignored in Null mode (no active session)", *Key, *Value);
    }
};
