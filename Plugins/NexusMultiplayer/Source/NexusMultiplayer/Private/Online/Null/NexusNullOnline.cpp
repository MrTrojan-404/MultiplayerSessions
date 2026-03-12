#include "Online/Null/NexusNullOnline.h"
#include "Core/NexusLog.h"
#include "OnlineSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "OnlineSessionSettings.h"
#include "Online/OnlineSessionNames.h"

FNexusNullOnline::FNexusNullOnline(UGameInstance* InGameInstance)
    : GameInstance(InGameInstance)
{
    NEXUS_LOG("[NullOnline] Created");
}

UWorld* FNexusNullOnline::GetWorld() const
{
    return GameInstance.IsValid() ? GameInstance->GetWorld() : nullptr;
}

FString FNexusNullOnline::GetDisplayName() const
{
    IOnlineSubsystem* OSS = IOnlineSubsystem::Get();
    if (!OSS) return FPlatformProcess::UserName(false);
    IOnlineIdentityPtr Identity = OSS->GetIdentityInterface();
    if (!Identity.IsValid()) return FPlatformProcess::UserName(false);
    FString Name = Identity->GetPlayerNickname(0);
    return Name.IsEmpty() ? FPlatformProcess::UserName(false) : Name;
}

FString FNexusNullOnline::GetUserId() const
{
    IOnlineSubsystem* OSS = IOnlineSubsystem::Get();
    if (!OSS) return FPlatformMisc::GetLoginId();
    IOnlineIdentityPtr Identity = OSS->GetIdentityInterface();
    if (!Identity.IsValid()) return FPlatformMisc::GetLoginId();
    TSharedPtr<const FUniqueNetId> Id = Identity->GetUniquePlayerId(0);
    return Id.IsValid() ? Id->ToString() : FPlatformMisc::GetLoginId();
}

FString FNexusNullOnline::GetAuthTicket() const
{
    return FString();
}

bool FNexusNullOnline::IsAvailable() const
{
    IOnlineSubsystem* OSS = IOnlineSubsystem::Get();
    return OSS != nullptr;
}

void FNexusNullOnline::CreateSession(const FNexusSessionConfig& Config)
{
    NEXUS_LOG("[NullOnline] CreateSession: Players=%d | LAN", Config.MaxPlayers);

    IOnlineSubsystem* OSS = IOnlineSubsystem::Get();
    if (!OSS) { if (OnCreateComplete) OnCreateComplete(false); return; }

    IOnlineSessionPtr Sessions = OSS->GetSessionInterface();
    if (!Sessions.IsValid()) { if (OnCreateComplete) OnCreateComplete(false); return; }

    // Destroy existing session first if present
    if (Sessions->GetNamedSession(NAME_GameSession))
    {
        Sessions->DestroySession(NAME_GameSession);
    }

    FOnlineSessionSettings Settings;
    Settings.bIsLANMatch            = true;
    Settings.NumPublicConnections   = Config.MaxPlayers;
    Settings.bAllowJoinInProgress   = true;
    Settings.bAllowJoinViaPresence  = true;
    Settings.bShouldAdvertise       = true;
    Settings.bUsesPresence          = true;
    Settings.bUseLobbiesIfAvailable = false;
    Settings.BuildUniqueId          = 1;
    Settings.Set(FName("MatchType"), Config.MatchType,
        EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

    // Bind create delegate
    OnCreateDelegateHandle = Sessions->AddOnCreateSessionCompleteDelegate_Handle(
        FOnCreateSessionCompleteDelegate::CreateLambda(
            [this, Sessions](FName, bool bSuccess)
            {
                Sessions->ClearOnCreateSessionCompleteDelegate_Handle(OnCreateDelegateHandle);
                NEXUS_LOG("[NullOnline] CreateSession complete: %s",
                    bSuccess ? TEXT("OK") : TEXT("FAIL"));
                if (OnCreateComplete) OnCreateComplete(bSuccess);
            }));

    UWorld* World = GetWorld();
    ULocalPlayer* LP = World
        ? World->GetFirstLocalPlayerFromController() : nullptr;
    TSharedPtr<const FUniqueNetId> PlayerId = LP
        ? LP->GetPreferredUniqueNetId().GetUniqueNetId() : nullptr;

    bool bCalled = false;
    if (PlayerId.IsValid())
        bCalled = Sessions->CreateSession(*PlayerId, NAME_GameSession, Settings);
    else
        bCalled = Sessions->CreateSession(0, NAME_GameSession, Settings);

    if (!bCalled)
    {
        Sessions->ClearOnCreateSessionCompleteDelegate_Handle(OnCreateDelegateHandle);
        if (OnCreateComplete) OnCreateComplete(false);
    }
}

void FNexusNullOnline::FindSessions(int32 MaxResults)
{
    NEXUS_LOG("[NullOnline] FindSessions: Max=%d", MaxResults);

    IOnlineSubsystem* OSS = IOnlineSubsystem::Get();
    if (!OSS) { if (OnFindComplete) OnFindComplete({}, false); return; }

    IOnlineSessionPtr Sessions = OSS->GetSessionInterface();
    if (!Sessions.IsValid()) { if (OnFindComplete) OnFindComplete({}, false); return; }

    TSharedRef<FOnlineSessionSearch> Search = MakeShared<FOnlineSessionSearch>();
    Search->MaxSearchResults = MaxResults;
    Search->bIsLanQuery      = true;

    OnFindDelegateHandle = Sessions->AddOnFindSessionsCompleteDelegate_Handle(
        FOnFindSessionsCompleteDelegate::CreateLambda(
            [this, Sessions, Search](bool bSuccess)
            {
                Sessions->ClearOnFindSessionsCompleteDelegate_Handle(OnFindDelegateHandle);
                TArray<FOnlineSessionSearchResult> Valid;
                if (bSuccess)
                    for (auto& R : Search->SearchResults)
                        if (R.IsValid()) Valid.Add(R);

                NEXUS_LOG("[NullOnline] FindSessions complete: %d found", Valid.Num());
                if (OnFindComplete) OnFindComplete(Valid, bSuccess);
            }));

    UWorld* World = GetWorld();
    ULocalPlayer* LP = World
        ? World->GetFirstLocalPlayerFromController() : nullptr;
    TSharedPtr<const FUniqueNetId> PlayerId = LP
        ? LP->GetPreferredUniqueNetId().GetUniqueNetId() : nullptr;

    bool bCalled = false;
    if (PlayerId.IsValid())
        bCalled = Sessions->FindSessions(*PlayerId, Search);
    else
        bCalled = Sessions->FindSessions(0, Search);

    if (!bCalled)
    {
        Sessions->ClearOnFindSessionsCompleteDelegate_Handle(OnFindDelegateHandle);
        if (OnFindComplete) OnFindComplete({}, false);
    }
}

void FNexusNullOnline::JoinSession(const FOnlineSessionSearchResult& Result)
{
    IOnlineSubsystem* OSS = IOnlineSubsystem::Get();
    if (!OSS) { if (OnJoinComplete) OnJoinComplete(false); return; }

    IOnlineSessionPtr Sessions = OSS->GetSessionInterface();
    if (!Sessions.IsValid()) { if (OnJoinComplete) OnJoinComplete(false); return; }

    OnJoinDelegateHandle = Sessions->AddOnJoinSessionCompleteDelegate_Handle(
        FOnJoinSessionCompleteDelegate::CreateLambda(
            [this, Sessions](FName, EOnJoinSessionCompleteResult::Type JoinResult)
            {
                Sessions->ClearOnJoinSessionCompleteDelegate_Handle(OnJoinDelegateHandle);
                const bool bOK = JoinResult == EOnJoinSessionCompleteResult::Success;
                NEXUS_LOG("[NullOnline] JoinSession complete: %s", bOK ? TEXT("OK") : TEXT("FAIL"));
                if (OnJoinComplete) OnJoinComplete(bOK);
            }));

    UWorld* World = GetWorld();
    ULocalPlayer* LP = World
        ? World->GetFirstLocalPlayerFromController() : nullptr;
    TSharedPtr<const FUniqueNetId> PlayerId = LP
        ? LP->GetPreferredUniqueNetId().GetUniqueNetId() : nullptr;

    bool bCalled = false;
    if (PlayerId.IsValid())
        bCalled = Sessions->JoinSession(*PlayerId, NAME_GameSession, Result);
    else
        bCalled = Sessions->JoinSession(0, NAME_GameSession, Result);

    if (!bCalled)
    {
        Sessions->ClearOnJoinSessionCompleteDelegate_Handle(OnJoinDelegateHandle);
        if (OnJoinComplete) OnJoinComplete(false);
    }
}

void FNexusNullOnline::JoinSessionById(const FString& SessionId)
{
    // LAN sessions don't have a meaningful ID lookup — just fail cleanly
    NEXUS_WARN("[NullOnline] JoinSessionById not supported on LAN — use server browser");
    if (OnJoinComplete) OnJoinComplete(false);
}

void FNexusNullOnline::FindSessionByAttribute(const FString& Key, const FString& Value)
{
    NEXUS_WARN("[NullOnline] FindSessionByAttribute not supported on LAN");
    if (OnAttributeSessionFound) OnAttributeSessionFound(FString());
}

void FNexusNullOnline::DestroySession()
{
    IOnlineSubsystem* OSS = IOnlineSubsystem::Get();
    if (!OSS) { if (OnDestroyComplete) OnDestroyComplete(false); return; }

    IOnlineSessionPtr Sessions = OSS->GetSessionInterface();
    if (!Sessions.IsValid()) { if (OnDestroyComplete) OnDestroyComplete(false); return; }

    OnDestroyDelegateHandle = Sessions->AddOnDestroySessionCompleteDelegate_Handle(
        FOnDestroySessionCompleteDelegate::CreateLambda(
            [this, Sessions](FName, bool bSuccess)
            {
                Sessions->ClearOnDestroySessionCompleteDelegate_Handle(OnDestroyDelegateHandle);
                NEXUS_LOG("[NullOnline] DestroySession complete: %s",
                    bSuccess ? TEXT("OK") : TEXT("FAIL"));
                if (OnDestroyComplete) OnDestroyComplete(bSuccess);
            }));

    if (!Sessions->DestroySession(NAME_GameSession))
    {
        Sessions->ClearOnDestroySessionCompleteDelegate_Handle(OnDestroyDelegateHandle);
        if (OnDestroyComplete) OnDestroyComplete(false);
    }
}
void FNexusNullOnline::SetSessionAttribute(const FString& Key, const FString& Value)
{
    // LAN sessions don't support attributes — no-op
    NEXUS_WARN("[NullOnline] SetSessionAttribute not supported on LAN (Key='%s')", *Key);
}