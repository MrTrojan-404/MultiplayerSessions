// Copyright NexusMultiplayer. All Rights Reserved.

#include "Backend/Platform/NexusPlatformBackend.h"
#include "Core/NexusLog.h"

FNexusPlatformBackend::FNexusPlatformBackend(
    TSharedPtr<INexusOnlineInterface> InOnline)
    : Online(InOnline)
{
    NEXUS_LOG("[PlatformBackend] Created. Online valid: %s",
        Online.IsValid() ? TEXT("YES") : TEXT("NO"));
}

void FNexusPlatformBackend::Login()
{
    // Platform is already logged in via Steam/EOS before the game launches.
    // Nothing to do — fire OnAccountLoaded immediately so the subsystem
    // continues its init flow without waiting.
    NEXUS_LOG("[PlatformBackend] Login called — platform already authenticated");

    if (OnAccountLoaded)
        OnAccountLoaded();
}

bool FNexusPlatformBackend::IsLoggedIn() const
{
    // As long as the online interface is available and the platform is running,
    // we consider ourselves "logged in"
    return Online.IsValid() && Online->IsAvailable();
}

FString FNexusPlatformBackend::GetDisplayName() const
{
    if (!Online.IsValid())
    {
        NEXUS_WARN("[PlatformBackend] GetDisplayName: Online interface null");
        return FPlatformProcess::UserName(false);
    }

    FString Name = Online->GetDisplayName();

    if (Name.IsEmpty())
    {
        // Fallback to OS username — always available
        Name = FPlatformProcess::UserName(false);
        NEXUS_WARN("[PlatformBackend] Platform name empty, using OS username: %s", *Name);
    }

    return Name;
}

FString FNexusPlatformBackend::GetBackendUserId() const
{
    // No backend user ID in platform-only mode — use platform ID
    if (!Online.IsValid()) return FString();
    return Online->GetUserId();
}

void FNexusPlatformBackend::SetDisplayName(const FString& NewName)
{
    // Platform display names are set through the platform's own UI
    // (Steam settings, EOS profile) — we can't change them from the game.
    // Fire callback immediately so the subsystem flow doesn't stall.
    NEXUS_WARN("[PlatformBackend] SetDisplayName: cannot set platform display name from game. Name: %s", *NewName);

    if (OnDisplayNameUpdated)
        OnDisplayNameUpdated(GetDisplayName()); // return actual platform name
}

void FNexusPlatformBackend::FetchAccount()
{
    // No async fetch needed — platform identity is always available.
    // Fire immediately with current platform name.
    FString Name = GetDisplayName();

    NEXUS_SUCCESS("[PlatformBackend] FetchAccount complete. Name: %s", *Name);

    if (OnDisplayNameUpdated)
        OnDisplayNameUpdated(Name);

    if (OnAccountLoaded)
        OnAccountLoaded();
}

void FNexusPlatformBackend::SubmitScore(
    const FString& LeaderboardId, int64 Score)
{
    // TODO: implement via Steam/EOS leaderboard API
    NEXUS_WARN("[PlatformBackend] SubmitScore not yet implemented. Board: %s | Score: %lld",
        *LeaderboardId, Score);
}

void FNexusPlatformBackend::FetchTopScores(
    const FString& LeaderboardId, int32 Limit)
{
    // TODO: implement via Steam/EOS leaderboard API
    NEXUS_WARN("[PlatformBackend] FetchTopScores not yet implemented. Board: %s | Limit: %d",
        *LeaderboardId, Limit);
}