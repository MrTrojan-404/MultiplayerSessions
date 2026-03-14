// Copyright NexusMultiplayer. All Rights Reserved.

#include "Online/EOS/NexusEOSOnline.h"
#include "Core/NexusLog.h"
#include "OnlineSubsystemUtils.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Engine/GameInstance.h"
#include "Online/OnlineSessionNames.h"

static const FName NexusEOSSessionName = FName("NexusSession");

// ── Constructor / Destructor ──────────────────────────────────────────────────

FNexusEOSOnline::FNexusEOSOnline(UGameInstance* InGameInstance)
    : GameInstance(InGameInstance)
{
    NEXUS_LOG("[EOSOnline] Created. GameInstance valid: %s",
        GameInstance.IsValid() ? TEXT("YES") : TEXT("NO"));

    OnCreateDelegate  = FOnCreateSessionCompleteDelegate ::CreateRaw(this, &FNexusEOSOnline::HandleCreateSessionComplete);
    OnFindDelegate    = FOnFindSessionsCompleteDelegate  ::CreateRaw(this, &FNexusEOSOnline::HandleFindSessionsComplete);
    OnJoinDelegate    = FOnJoinSessionCompleteDelegate   ::CreateRaw(this, &FNexusEOSOnline::HandleJoinSessionComplete);
    OnStartDelegate   = FOnStartSessionCompleteDelegate  ::CreateRaw(this, &FNexusEOSOnline::HandleStartSessionComplete);
    OnDestroyDelegate = FOnDestroySessionCompleteDelegate::CreateRaw(this, &FNexusEOSOnline::HandleDestroySessionComplete);
}

FNexusEOSOnline::~FNexusEOSOnline()
{
    // Clear delegate handles before releasing session interface
    if (SessionInterface.IsValid())
    {
        SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionHandle);
        SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsHandle);
        SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionHandle);
        SessionInterface->ClearOnStartSessionCompleteDelegate_Handle(StartSessionHandle);
        SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionHandle);
    }
    SessionInterface.Reset();
}

// ── Identity ──────────────────────────────────────────────────────────────────

IOnlineSubsystem* FNexusEOSOnline::GetEOSOSS() const
{
    return IOnlineSubsystem::Get(FName("EOS"));
}

bool FNexusEOSOnline::IsAvailable() const
{
    IOnlineSubsystem* OSS = GetEOSOSS();
    if (!OSS) return false;
    IOnlineIdentityPtr Identity = OSS->GetIdentityInterface();
    if (!Identity.IsValid()) return false;
    return Identity->GetLoginStatus(0) != ELoginStatus::NotLoggedIn;
}

FString FNexusEOSOnline::GetDisplayName() const
{
    IOnlineSubsystem* OSS = GetEOSOSS();
    if (!OSS) return FString();
    IOnlineIdentityPtr Identity = OSS->GetIdentityInterface();
    if (!Identity.IsValid()) return FString();
    return Identity->GetPlayerNickname(0);
}

FString FNexusEOSOnline::GetUserId() const
{
    IOnlineSubsystem* OSS = GetEOSOSS();
    if (!OSS) return FString();
    IOnlineIdentityPtr Identity = OSS->GetIdentityInterface();
    if (!Identity.IsValid()) return FString();
    TSharedPtr<const FUniqueNetId> Id = Identity->GetUniquePlayerId(0);
    return Id.IsValid() ? Id->ToString() : FString();
}

FString FNexusEOSOnline::GetAuthTicket() const
{
    IOnlineSubsystem* OSS = GetEOSOSS();
    if (!OSS) return FString();
    IOnlineIdentityPtr Identity = OSS->GetIdentityInterface();
    if (!Identity.IsValid()) return FString();
    return Identity->GetAuthToken(0);
}

// ── Sessions ──────────────────────────────────────────────────────────────────

void FNexusEOSOnline::CreateSession(const FNexusSessionConfig& Config)
{
    NEXUS_LOG("[EOSOnline] CreateSession: Players=%d | Type=%s | Private=%s | LAN=%s",
        Config.MaxPlayers, *Config.MatchType,
        Config.bPrivate ? TEXT("YES") : TEXT("NO"),
        bIsLAN ? TEXT("YES") : TEXT("NO"));

    if (!EnsureSessionInterface()) return;

    // Destroy existing session first if one exists
    FNamedOnlineSession* Existing = SessionInterface->GetNamedSession(NexusEOSSessionName);
    if (Existing)
    {
        NEXUS_LOG("[EOSOnline] Existing session found — destroying before recreate");
        bCreateOnDestroy            = true;
        PendingNumPublicConnections = Config.MaxPlayers;
        PendingMatchType            = Config.MatchType;
        bPendingPrivate             = Config.bPrivate;
        DestroySessionHandle =
            SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(OnDestroyDelegate);
        SessionInterface->DestroySession(NexusEOSSessionName);
        return;
    }

    DesiredNumPublicConnections = Config.MaxPlayers;
    DesiredMatchType            = Config.MatchType;
    bLastSessionWasPrivate      = Config.bPrivate;

    CreateSessionInternal(Config.MaxPlayers, Config.MatchType, Config.bPrivate);
}

void FNexusEOSOnline::CreateSessionInternal(
    int32 NumPlayers, const FString& MatchType, bool bPrivate)
{
    TSharedPtr<const FUniqueNetId> PlayerId = GetLocalPlayerId();
    if (!PlayerId.IsValid())
    {
        NEXUS_ERROR("[EOSOnline] CreateSessionInternal: no valid player ID");
        if (OnCreateComplete) OnCreateComplete(false);
        return;
    }

    NEXUS_LOG("[EOSOnline] CreateSessionInternal: Players=%d | LAN=%s",
        NumPlayers, bIsLAN ? TEXT("YES") : TEXT("NO"));

    CreateSessionHandle =
        SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(OnCreateDelegate);

    LastSessionSettings = MakeShareable(new FOnlineSessionSettings());
    LastSessionSettings->bIsLANMatch             = bIsLAN;
    LastSessionSettings->NumPublicConnections     = NumPlayers;
    LastSessionSettings->bAllowJoinInProgress     = true;
    LastSessionSettings->bAllowJoinViaPresence    = !bIsLAN && !bPrivate;
    LastSessionSettings->bShouldAdvertise         = !bPrivate;
    LastSessionSettings->bAllowJoinViaPresenceFriendsOnly = false;
    LastSessionSettings->bUsesPresence            = !bIsLAN;
    LastSessionSettings->bUseLobbiesIfAvailable   = !bIsLAN; // EOS supports lobbies too
    LastSessionSettings->BuildUniqueId = 1;
    LastSessionSettings->Set(
    FName("MapName"),
    FString("Lobby"),
    EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

    LastSessionSettings->Set(
        FName("MatchType"), MatchType,
        bIsLAN
            ? EOnlineDataAdvertisementType::ViaPingOnly
            : EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

    if (!SessionInterface->CreateSession(*PlayerId, NexusEOSSessionName, *LastSessionSettings))
    {
        NEXUS_ERROR("[EOSOnline] CreateSession call failed immediately");
        SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionHandle);
        if (OnCreateComplete) OnCreateComplete(false);
    }
}

void FNexusEOSOnline::FindSessions(int32 MaxResults)
{
    NEXUS_LOG("[EOSOnline] FindSessions: Max=%d | LAN=%s",
        MaxResults, bIsLAN ? TEXT("YES") : TEXT("NO"));

    if (!EnsureSessionInterface()) return;

    TSharedPtr<const FUniqueNetId> PlayerId = GetLocalPlayerId();
    if (!PlayerId.IsValid())
    {
        if (OnFindComplete) OnFindComplete({}, false);
        return;
    }

    FindSessionsHandle =
        SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(OnFindDelegate);

    LastSessionSearch = MakeShareable(new FOnlineSessionSearch());
    LastSessionSearch->MaxSearchResults = FMath::Clamp(MaxResults, 1, 200);
    LastSessionSearch->bIsLanQuery      = bIsLAN;

    // EOS presence-based search — no SEARCH_LOBBIES filter needed unlike Steam
    /*if (!bIsLAN)
        LastSessionSearch->QuerySettings.Set(
            SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);*/

    if (!SessionInterface->FindSessions(*PlayerId, LastSessionSearch.ToSharedRef()))
    {
        NEXUS_ERROR("[EOSOnline] FindSessions call failed immediately");
        SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsHandle);
        if (OnFindComplete) OnFindComplete({}, false);
    }
}

void FNexusEOSOnline::FindSessionByAttribute(const FString& Key, const FString& Value)
{
    NEXUS_LOG("[EOSOnline] FindSessionByAttribute: %s = %s", *Key, *Value);

    if (!EnsureSessionInterface()) return;

    TSharedPtr<const FUniqueNetId> PlayerId = GetLocalPlayerId();
    if (!PlayerId.IsValid())
    {
        if (OnAttributeSessionFound) OnAttributeSessionFound(FString());
        return;
    }

    bFindingByAttribute   = true;
    PendingAttributeKey   = Key;
    PendingAttributeValue = Value;

    FindSessionsHandle =
        SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(OnFindDelegate);

    LastSessionSearch = MakeShareable(new FOnlineSessionSearch());
    LastSessionSearch->MaxSearchResults = 200;
    LastSessionSearch->bIsLanQuery      = bIsLAN;
    LastSessionSearch->QuerySettings.Set(
        FName(*Key), Value, EOnlineComparisonOp::Equals);

    if (!SessionInterface->FindSessions(*PlayerId, LastSessionSearch.ToSharedRef()))
    {
        bFindingByAttribute = false;
        SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsHandle);
        if (OnAttributeSessionFound) OnAttributeSessionFound(FString());
    }
}

void FNexusEOSOnline::JoinSession(const FOnlineSessionSearchResult& Result)
{
    NEXUS_LOG("[EOSOnline] JoinSession");

    if (!EnsureSessionInterface()) return;

    TSharedPtr<const FUniqueNetId> PlayerId = GetLocalPlayerId();
    if (!PlayerId.IsValid())
    {
        if (OnJoinComplete) OnJoinComplete(false);
        return;
    }

    JoinSessionHandle =
        SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(OnJoinDelegate);

    if (!SessionInterface->JoinSession(*PlayerId, NexusEOSSessionName, Result))
    {
        NEXUS_ERROR("[EOSOnline] JoinSession call failed immediately");
        SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionHandle);
        if (OnJoinComplete) OnJoinComplete(false);
    }
}

void FNexusEOSOnline::JoinSessionById(const FString& SessionId)
{
    NEXUS_LOG("[EOSOnline] JoinSessionById: %s", *SessionId);

    if (!LastSessionSearch.IsValid())
    {
        NEXUS_ERROR("[EOSOnline] JoinSessionById: no cached search results");
        if (OnJoinComplete) OnJoinComplete(false);
        return;
    }

    FOnlineSessionSearchResult Found;
    if (!GetSearchResultById(SessionId, Found))
    {
        NEXUS_ERROR("[EOSOnline] JoinSessionById: session '%s' not in cached results", *SessionId);
        if (OnJoinComplete) OnJoinComplete(false);
        return;
    }

    JoinSession(Found);
}

void FNexusEOSOnline::DestroySession()
{
    NEXUS_LOG("[EOSOnline] DestroySession");

    if (!EnsureSessionInterface()) return;

    FNamedOnlineSession* Session = SessionInterface->GetNamedSession(NexusEOSSessionName);
    if (!Session)
    {
        NEXUS_WARN("[EOSOnline] DestroySession: no active session to destroy");
        if (OnDestroyComplete) OnDestroyComplete(true);
        return;
    }

    DestroySessionHandle =
        SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(OnDestroyDelegate);

    if (!SessionInterface->DestroySession(NexusEOSSessionName))
    {
        NEXUS_ERROR("[EOSOnline] DestroySession call failed immediately");
        SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionHandle);
        if (OnDestroyComplete) OnDestroyComplete(false);
    }
}

void FNexusEOSOnline::SetSessionAttribute(const FString& Key, const FString& Value)
{
    NEXUS_LOG("[EOSOnline] SetSessionAttribute: %s = %s", *Key, *Value);

    if (!EnsureSessionInterface()) return;

    FNamedOnlineSession* Session = SessionInterface->GetNamedSession(NexusEOSSessionName);
    if (!Session)
    {
        NEXUS_WARN("[EOSOnline] SetSessionAttribute: no active session");
        return;
    }

    Session->SessionSettings.Set(
        FName(*Key), Value,
        EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

    SessionInterface->UpdateSession(NexusEOSSessionName, Session->SessionSettings);
}

// ── Callbacks ─────────────────────────────────────────────────────────────────

void FNexusEOSOnline::HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
    SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionHandle);

    NEXUS_LOG("[EOSOnline] HandleCreateSessionComplete: %s", bWasSuccessful ? TEXT("OK") : TEXT("FAIL"));

    if (bWasSuccessful)
        StartSession();
    else if (OnCreateComplete)
        OnCreateComplete(false);
}

void FNexusEOSOnline::HandleStartSessionComplete(FName SessionName, bool bWasSuccessful)
{
    SessionInterface->ClearOnStartSessionCompleteDelegate_Handle(StartSessionHandle);

    NEXUS_LOG("[EOSOnline] HandleStartSessionComplete: %s", bWasSuccessful ? TEXT("OK") : TEXT("FAIL"));

    if (OnCreateComplete) OnCreateComplete(bWasSuccessful);
}

void FNexusEOSOnline::HandleFindSessionsComplete(bool bWasSuccessful)
{
    SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsHandle);

    NEXUS_LOG("[EOSOnline] HandleFindSessionsComplete: %s | Results: %d",
        bWasSuccessful ? TEXT("OK") : TEXT("FAIL"),
        LastSessionSearch.IsValid() ? LastSessionSearch->SearchResults.Num() : 0);

    if (bFindingByAttribute)
    {
        bFindingByAttribute = false;

        if (!bWasSuccessful || !LastSessionSearch.IsValid())
        {
            if (OnAttributeSessionFound) OnAttributeSessionFound(FString());
            return;
        }

        // Find the result matching the attribute value
        for (const FOnlineSessionSearchResult& Result : LastSessionSearch->SearchResults)
        {
            FString Val;
            if (Result.Session.SessionSettings.Get(FName(*PendingAttributeKey), Val)
                && Val == PendingAttributeValue)
            {
                // Return session ID so subsystem can call JoinSessionById
                const FString SessionId = Result.GetSessionIdStr();
                NEXUS_SUCCESS("[EOSOnline] Attribute match found. SessionId: %s", *SessionId);
                if (OnAttributeSessionFound) OnAttributeSessionFound(SessionId);
                return;
            }
        }

        NEXUS_WARN("[EOSOnline] No session found with %s = %s",
            *PendingAttributeKey, *PendingAttributeValue);
        if (OnAttributeSessionFound) OnAttributeSessionFound(FString());
        return;
    }

    if (OnFindComplete)
    {
        const TArray<FOnlineSessionSearchResult>& Results =
            LastSessionSearch.IsValid() ? LastSessionSearch->SearchResults : TArray<FOnlineSessionSearchResult>();
        OnFindComplete(Results, bWasSuccessful);
    }
}

void FNexusEOSOnline::HandleJoinSessionComplete(
    FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
    SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionHandle);

    const bool bOK = Result == EOnJoinSessionCompleteResult::Success;

    NEXUS_LOG("[EOSOnline] HandleJoinSessionComplete: %s (code=%d)",
        bOK ? TEXT("OK") : TEXT("FAIL"), (int32)Result);

    if (OnJoinComplete) OnJoinComplete(bOK);
}

void FNexusEOSOnline::HandleDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
    SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionHandle);

    NEXUS_LOG("[EOSOnline] HandleDestroySessionComplete: %s", bWasSuccessful ? TEXT("OK") : TEXT("FAIL"));

    if (bCreateOnDestroy)
    {
        bCreateOnDestroy = false;
        NEXUS_LOG("[EOSOnline] Recreating session after destroy");
        CreateSessionInternal(PendingNumPublicConnections, PendingMatchType, bPendingPrivate);
        return;
    }

    if (OnDestroyComplete) OnDestroyComplete(bWasSuccessful);
}

// ── Helpers ───────────────────────────────────────────────────────────────────

bool FNexusEOSOnline::EnsureSessionInterface()
{
    if (SessionInterface.IsValid()) return true;

    IOnlineSubsystem* OSS = GetEOSOSS();
    if (!OSS)
    {
        NEXUS_ERROR("[EOSOnline] EOS subsystem not found — is OnlineSubsystemEOS enabled?");
        return false;
    }

    SessionInterface = OSS->GetSessionInterface();
    if (!SessionInterface.IsValid())
    {
        NEXUS_ERROR("[EOSOnline] EOS session interface invalid");
        return false;
    }

    return true;
}

TSharedPtr<const FUniqueNetId> FNexusEOSOnline::GetLocalPlayerId() const
{
    IOnlineSubsystem* OSS = GetEOSOSS();
    if (!OSS) return nullptr;
    IOnlineIdentityPtr Identity = OSS->GetIdentityInterface();
    if (!Identity.IsValid()) return nullptr;
    return Identity->GetUniquePlayerId(0);
}

void FNexusEOSOnline::StartSession()
{
    NEXUS_LOG("[EOSOnline] StartSession");
    StartSessionHandle =
        SessionInterface->AddOnStartSessionCompleteDelegate_Handle(OnStartDelegate);

    if (!SessionInterface->StartSession(NexusEOSSessionName))
    {
        NEXUS_ERROR("[EOSOnline] StartSession call failed immediately");
        SessionInterface->ClearOnStartSessionCompleteDelegate_Handle(StartSessionHandle);
        if (OnCreateComplete) OnCreateComplete(false);
    }
}

FString FNexusEOSOnline::GetCurrentSessionId() const
{
    if (!SessionInterface.IsValid()) return FString();
    FNamedOnlineSession* Session = SessionInterface->GetNamedSession(NexusEOSSessionName);
    if (!Session || !Session->SessionInfo.IsValid()) return FString();
    return Session->GetSessionIdStr();
}

bool FNexusEOSOnline::GetSearchResultById(
    const FString& SessionId, FOnlineSessionSearchResult& OutResult) const
{
    if (!LastSessionSearch.IsValid()) return false;
    for (const FOnlineSessionSearchResult& R : LastSessionSearch->SearchResults)
        if (R.GetSessionIdStr() == SessionId)
        { OutResult = R; return true; }
    return false;
}