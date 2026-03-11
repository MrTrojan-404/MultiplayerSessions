// Copyright NexusMultiplayer. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Interfaces/INexusBackendInterface.h"
#include "Core/NexusLog.h"

/**
 * FNexusNullBackend
 *
 * Safe no-op backend for Null online mode (LAN / dev / PIE).
 * No server, no auth — just fires all callbacks immediately so the
 * subsystem init flow completes without hanging.
 *
 * GetDisplayName() returns the OS username so the player always has a name.
 */
class NEXUSMULTIPLAYER_API FNexusNullBackend : public INexusBackendInterface
{
public:

    FNexusNullBackend()
    {
        NEXUS_LOG("[NullBackend] Created — no backend features available in Null mode");
    }

    virtual void Login() override
    {
        NEXUS_LOG("[NullBackend] Login called — no-op, firing OnAccountLoaded immediately");
        if (OnAccountLoaded) OnAccountLoaded();
    }

    virtual bool IsLoggedIn() const override
    {
        // Always return true so IsBackendConnected() doesn't block UI
        return true;
    }

    virtual ENexusBackendMode GetMode() const override
    {
        return ENexusBackendMode::PlatformOnly;
    }

    virtual FString GetDisplayName() const override
    {
        FString Name = FPlatformProcess::UserName(false);
        if (Name.IsEmpty())
        {
            NEXUS_WARN("[NullBackend] OS username empty — using fallback 'Player'");
            return TEXT("Player");
        }
        return Name;
    }

    virtual FString GetBackendUserId() const override
    {
        // No backend user ID — use stable machine ID as fallback
        return FPlatformMisc::GetLoginId();
    }

    virtual void SetDisplayName(const FString& NewName) override
    {
        // No backend to persist to — fire callback immediately with the requested name
        NEXUS_WARN("[NullBackend] SetDisplayName: no backend to persist to. "
                   "Name '%s' will only last this session.", *NewName);
        if (OnDisplayNameUpdated) OnDisplayNameUpdated(NewName);
    }

    virtual void FetchAccount() override
    {
        FString Name = GetDisplayName();
        NEXUS_LOG("[NullBackend] FetchAccount — returning OS username: '%s'", *Name);
        if (OnDisplayNameUpdated) OnDisplayNameUpdated(Name);
        if (OnAccountLoaded) OnAccountLoaded();
    }

    virtual void SubmitScore(const FString& LeaderboardId, int64 Score) override
    {
        NEXUS_WARN("[NullBackend] SubmitScore ignored in Null mode. "
                   "Board='%s' Score=%lld", *LeaderboardId, Score);
    }

    virtual void FetchTopScores(const FString& LeaderboardId, int32 Limit) override
    {
        NEXUS_WARN("[NullBackend] FetchTopScores ignored in Null mode. "
                   "Board='%s' Limit=%d", *LeaderboardId, Limit);
    }
};