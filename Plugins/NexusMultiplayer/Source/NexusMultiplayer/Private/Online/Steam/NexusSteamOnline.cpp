// Copyright NexusMultiplayer. All Rights Reserved.

#include "Online/Steam/NexusSteamOnline.h"
#include "Core/NexusLog.h"
#include "OnlineSubsystemUtils.h"
#include "Core/NexusMultiplayerSettings.h"
#include "Online/OnlineSessionNames.h"
#include "Interfaces/OnlineIdentityInterface.h"

FNexusSteamOnline::FNexusSteamOnline(UGameInstance* InGameInstance)
    : GameInstance(InGameInstance)
{
    OnCreateDelegate  = FOnCreateSessionCompleteDelegate::CreateRaw(
        this, &FNexusSteamOnline::HandleCreateSessionComplete);
    OnFindDelegate    = FOnFindSessionsCompleteDelegate::CreateRaw(
        this, &FNexusSteamOnline::HandleFindSessionsComplete);
    OnJoinDelegate    = FOnJoinSessionCompleteDelegate::CreateRaw(
        this, &FNexusSteamOnline::HandleJoinSessionComplete);
    OnStartDelegate   = FOnStartSessionCompleteDelegate::CreateRaw(
        this, &FNexusSteamOnline::HandleStartSessionComplete);
    OnDestroyDelegate = FOnDestroySessionCompleteDelegate::CreateRaw(
        this, &FNexusSteamOnline::HandleDestroySessionComplete);
}

FNexusSteamOnline::~FNexusSteamOnline()
{
    if (SessionInterface.IsValid())
    {
        SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionHandle);
        SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsHandle);
        SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionHandle);
        SessionInterface->ClearOnStartSessionCompleteDelegate_Handle(StartSessionHandle);
        SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionHandle);
    }
}

// ── Identity ──────────────────────────────────────────────────────────────────

FString FNexusSteamOnline::GetDisplayName() const
{
    IOnlineSubsystem* OSS = IOnlineSubsystem::Get(FName("Steam"));
    if (!OSS) return FString();
    IOnlineIdentityPtr Identity = OSS->GetIdentityInterface();
    if (!Identity.IsValid()) return FString();
    return Identity->GetPlayerNickname(0);
}

FString FNexusSteamOnline::GetUserId() const
{
    IOnlineSubsystem* OSS = IOnlineSubsystem::Get(FName("Steam"));
    if (!OSS) return FString();
    IOnlineIdentityPtr Identity = OSS->GetIdentityInterface();
    if (!Identity.IsValid()) return FString();
    TSharedPtr<const FUniqueNetId> Id = Identity->GetUniquePlayerId(0);
    return Id.IsValid() ? Id->ToString() : FString();
}
FString FNexusSteamOnline::GetAuthTicket() const
{
    IOnlineSubsystem* OSS = Online::GetSubsystem(GetWorld());
    if (!OSS) return FString();
    IOnlineIdentityPtr Identity = OSS->GetIdentityInterface();
    if (!Identity.IsValid()) return FString();
    return Identity->GetAuthToken(0);
}

bool FNexusSteamOnline::IsAvailable() const
{
    IOnlineSubsystem* OSS = IOnlineSubsystem::Get(FName("Steam"));
    if (!OSS) return false;

    // OSS loads even without Steam client running — verify client is actually up
    IOnlineIdentityPtr Identity = OSS->GetIdentityInterface();
    if (!Identity.IsValid()) return false;

    const ELoginStatus::Type Status = Identity->GetLoginStatus(0);
    return Status != ELoginStatus::NotLoggedIn;
}
// ── Create ────────────────────────────────────────────────────────────────────

void FNexusSteamOnline::CreateSession(const FNexusSessionConfig& Config)
{
    DesiredNumPublicConnections = Config.MaxPlayers;
    DesiredMatchType            = Config.MatchType;
    bLastSessionWasPrivate      = Config.bPrivate;

    if (!EnsureSessionInterface())
    {
        if (OnCreateComplete) OnCreateComplete(false);
        return;
    }

    if (SessionInterface->GetNamedSession(NAME_GameSession) != nullptr)
    {
        bCreateOnDestroy            = true;
        PendingNumPublicConnections = Config.MaxPlayers;
        PendingMatchType            = Config.MatchType;
        bPendingPrivate             = Config.bPrivate;
        DestroySession();
        return;
    }

    CreateSessionInternal(Config.MaxPlayers, Config.MatchType, Config.bPrivate);
}

void FNexusSteamOnline::CreateSessionInternal(
    int32 NumPlayers, const FString& MatchType, bool bPrivate)
{
    NEXUS_LOG("[SteamOnline] CreateSessionInternal: Players=%d | Type='%s' | Private=%s",
        NumPlayers, *MatchType, bPrivate ? TEXT("YES") : TEXT("NO"));

    if (!EnsureSessionInterface())
    {
        if (OnCreateComplete) OnCreateComplete(false);
        return;
    }

    TSharedPtr<const FUniqueNetId> PlayerId = GetLocalPlayerId();
    if (!PlayerId.IsValid())
    {
        if (OnCreateComplete) OnCreateComplete(false);
        return;
    }
    
    NEXUS_LOG("[SteamOnline] CreateSessionInternal: LAN=%s", bIsLAN ? TEXT("YES") : TEXT("NO"));

    CreateSessionHandle =
        SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(OnCreateDelegate);

    LastSessionSettings = MakeShareable(new FOnlineSessionSettings());
    LastSessionSettings->bIsLANMatch = bIsLAN;
    LastSessionSettings->NumPublicConnections = NumPlayers;
    LastSessionSettings->bAllowJoinInProgress = true;
    LastSessionSettings->bAllowJoinViaPresence = !bIsLAN && !bPrivate;
    LastSessionSettings->bShouldAdvertise = !bPrivate;
    LastSessionSettings->bUsesPresence = !bIsLAN;
    LastSessionSettings->bUseLobbiesIfAvailable = !bIsLAN;
    LastSessionSettings->BuildUniqueId = 1;
    LastSessionSettings->Set(FName("MatchType"), MatchType,
                             bIsLAN
                                 ? EOnlineDataAdvertisementType::ViaPingOnly
                                 : EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
    LastSessionSettings->Set(
    FName("MapName"),
    FString("Lobby"),
    EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

    FString DisplayName = GetDisplayName();
    if (!DisplayName.IsEmpty())
        LastSessionSettings->Set(FName("HostDisplayName"), DisplayName,
                                 bIsLAN
                                     ? EOnlineDataAdvertisementType::ViaPingOnly
                                     : EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

    const bool bCalled = SessionInterface->CreateSession(
        *PlayerId, NAME_GameSession, *LastSessionSettings);

    if (!bCalled)
    {
        SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionHandle);
        if (OnCreateComplete) OnCreateComplete(false);
    }
}

// ── Find ──────────────────────────────────────────────────────────────────────

void FNexusSteamOnline::FindSessions(int32 MaxResults)
{
    bFindingBySessionId = false;
    bFindingByAttribute = false;

    if (!EnsureSessionInterface()) { if (OnFindComplete) OnFindComplete({}, false); return; }
    TSharedPtr<const FUniqueNetId> PlayerId = GetLocalPlayerId();
    if (!PlayerId.IsValid()) { if (OnFindComplete) OnFindComplete({}, false); return; }

    // ── LAN detection — use settings flag, NOT OSS name ──────────────────────
    const UNexusMultiplayerSettings* S = UNexusMultiplayerSettings::Get();

    NEXUS_LOG("[SteamOnline] FindSessions: Max=%d | LAN=%s",
        MaxResults, bIsLAN ? TEXT("YES") : TEXT("NO"));

    FindSessionsHandle =
        SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(OnFindDelegate);

    LastSessionSearch = MakeShareable(new FOnlineSessionSearch());
    LastSessionSearch->MaxSearchResults = FMath::Clamp(MaxResults, 1, 200);

    LastSessionSearch->bIsLanQuery = bIsLAN;
    if (!bIsLAN)
        LastSessionSearch->QuerySettings.Set(
            SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
    
    if (!SessionInterface->FindSessions(*PlayerId, LastSessionSearch.ToSharedRef()))
    {
        SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsHandle);
        if (OnFindComplete) OnFindComplete({}, false);
    }
}

void FNexusSteamOnline::FindSessionByAttribute(const FString& Key, const FString& Value)
{
    bFindingByAttribute   = true;
    bFindingBySessionId   = false;
    PendingAttributeKey   = Key;
    PendingAttributeValue = Value;

    if (!EnsureSessionInterface()) { if (OnAttributeSessionFound) OnAttributeSessionFound(FString()); return; }
    TSharedPtr<const FUniqueNetId> PlayerId = GetLocalPlayerId();
    if (!PlayerId.IsValid()) { if (OnAttributeSessionFound) OnAttributeSessionFound(FString()); return; }

    FindSessionsHandle =
        SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(OnFindDelegate);
    
    LastSessionSearch = MakeShareable(new FOnlineSessionSearch());
    LastSessionSearch->MaxSearchResults = 200;
    LastSessionSearch->bIsLanQuery      = bIsLAN;
    if (!bIsLAN)
        LastSessionSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);

    if (!SessionInterface->FindSessions(*PlayerId, LastSessionSearch.ToSharedRef()))
    {
        SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsHandle);
        if (OnAttributeSessionFound) OnAttributeSessionFound(FString());
    }
}

void FNexusSteamOnline::JoinSessionById(const FString& SessionId)
{
    bFindingBySessionId  = true;
    bFindingByAttribute  = false;
    PendingFindSessionId = SessionId;

    if (!EnsureSessionInterface()) { if (OnJoinComplete) OnJoinComplete(false); return; }
    TSharedPtr<const FUniqueNetId> PlayerId = GetLocalPlayerId();
    if (!PlayerId.IsValid()) { if (OnJoinComplete) OnJoinComplete(false); return; }

    FindSessionsHandle =
        SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(OnFindDelegate);
    
    LastSessionSearch = MakeShareable(new FOnlineSessionSearch());
    LastSessionSearch->MaxSearchResults = 200;
    LastSessionSearch->bIsLanQuery      = bIsLAN;
    if (!bIsLAN)
        LastSessionSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);

    if (!SessionInterface->FindSessions(*PlayerId, LastSessionSearch.ToSharedRef()))
    {
        SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsHandle);
        if (OnJoinComplete) OnJoinComplete(false);
    }
}

// ── Join ──────────────────────────────────────────────────────────────────────

void FNexusSteamOnline::JoinSession(const FOnlineSessionSearchResult& Result)
{
    if (!EnsureSessionInterface()) { if (OnJoinComplete) OnJoinComplete(false); return; }
    TSharedPtr<const FUniqueNetId> PlayerId = GetLocalPlayerId();
    if (!PlayerId.IsValid()) { if (OnJoinComplete) OnJoinComplete(false); return; }

    JoinSessionHandle =
        SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(OnJoinDelegate);

    if (!SessionInterface->JoinSession(*PlayerId, NAME_GameSession, Result))
    {
        SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionHandle);
        if (OnJoinComplete) OnJoinComplete(false);
    }
}

// ── Destroy ───────────────────────────────────────────────────────────────────

void FNexusSteamOnline::DestroySession()
{
    if (!EnsureSessionInterface()) { if (OnDestroyComplete) OnDestroyComplete(false); return; }

    DestroySessionHandle =
        SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(OnDestroyDelegate);

    if (!SessionInterface->DestroySession(NAME_GameSession))
    {
        SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionHandle);
        if (OnDestroyComplete) OnDestroyComplete(false);
    }
}

void FNexusSteamOnline::StartSession()
{
    if (!EnsureSessionInterface()) return;
    StartSessionHandle =
        SessionInterface->AddOnStartSessionCompleteDelegate_Handle(OnStartDelegate);
    if (!SessionInterface->StartSession(NAME_GameSession))
        SessionInterface->ClearOnStartSessionCompleteDelegate_Handle(StartSessionHandle);
}

// ── Lobby Attribute ───────────────────────────────────────────────────────────

void FNexusSteamOnline::SetSessionAttribute(const FString& Key, const FString& Value)
{
    if (!EnsureSessionInterface()) return;
    FOnlineSessionSettings* Settings =
        SessionInterface->GetSessionSettings(NAME_GameSession);
    if (!Settings) return;

    Settings->Set(FName(*Key), Value, EOnlineDataAdvertisementType::ViaOnlineService);
    SessionInterface->UpdateSession(NAME_GameSession, *Settings);
}

// ── Callbacks ─────────────────────────────────────────────────────────────────

void FNexusSteamOnline::HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
    if (SessionInterface.IsValid())
        SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionHandle);

    if (bWasSuccessful) StartSession();
    if (OnCreateComplete) OnCreateComplete(bWasSuccessful);
}

void FNexusSteamOnline::HandleFindSessionsComplete(bool bWasSuccessful)
{
    if (SessionInterface.IsValid())
        SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsHandle);

    if (!LastSessionSearch.IsValid())
    {
        if (bFindingByAttribute)   { bFindingByAttribute = false; if (OnAttributeSessionFound) OnAttributeSessionFound(FString()); }
        else if (bFindingBySessionId) { bFindingBySessionId = false; if (OnJoinComplete) OnJoinComplete(false); }
        else { if (OnFindComplete) OnFindComplete({}, false); }
        return;
    }

    // Path A — FindByAttribute (Steam match code)
    if (bFindingByAttribute)
    {
        bFindingByAttribute = false;
        if (bWasSuccessful)
        {
            for (const FOnlineSessionSearchResult& Result : LastSessionSearch->SearchResults)
            {
                FString AttrValue;
                if (Result.Session.SessionSettings.Get(FName(*PendingAttributeKey), AttrValue)
                    && AttrValue == PendingAttributeValue)
                {
                    if (OnAttributeSessionFound) OnAttributeSessionFound(Result.GetSessionIdStr());
                    return;
                }
            }
        }
        if (OnAttributeSessionFound) OnAttributeSessionFound(FString());
        return;
    }

    // Path B — FindBySessionId (Nakama gave us the session ID)
    if (bFindingBySessionId)
    {
        bFindingBySessionId = false;
        if (bWasSuccessful)
        {
            for (const FOnlineSessionSearchResult& Result : LastSessionSearch->SearchResults)
            {
                if (Result.GetSessionIdStr() == PendingFindSessionId)
                {
                    PendingFindSessionId.Empty();
                    JoinSession(Result);
                    return;
                }
            }
        }
        PendingFindSessionId.Empty();
        if (OnJoinComplete) OnJoinComplete(false);
        return;
    }

    // Path C — Normal server browser
    TArray<FOnlineSessionSearchResult> ValidResults;
    for (const FOnlineSessionSearchResult& Result : LastSessionSearch->SearchResults)
        if (Result.IsValid() && Result.Session.NumOpenPublicConnections > 0)
            ValidResults.Add(Result);

    if (OnFindComplete) OnFindComplete(ValidResults, bWasSuccessful);
}

void FNexusSteamOnline::HandleJoinSessionComplete(
    FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
    if (SessionInterface.IsValid())
        SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionHandle);
    if (OnJoinComplete) OnJoinComplete(Result == EOnJoinSessionCompleteResult::Success);
}

void FNexusSteamOnline::HandleStartSessionComplete(FName SessionName, bool bWasSuccessful)
{
    if (SessionInterface.IsValid())
        SessionInterface->ClearOnStartSessionCompleteDelegate_Handle(StartSessionHandle);
}

void FNexusSteamOnline::HandleDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
    if (SessionInterface.IsValid())
        SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionHandle);

    if (bWasSuccessful && bCreateOnDestroy)
    {
        bCreateOnDestroy = false;
        CreateSessionInternal(PendingNumPublicConnections, PendingMatchType, bPendingPrivate);
        return;
    }
    if (OnDestroyComplete) OnDestroyComplete(bWasSuccessful);
}

// ── Helpers ───────────────────────────────────────────────────────────────────

// EnsureSessionInterface — replace Online::GetSubsystem with direct Get()
bool FNexusSteamOnline::EnsureSessionInterface()
{
    // Use IOnlineSubsystem::Get(FName("Steam")) directly — more reliable in packaged builds
    // than the world-context version
    IOnlineSubsystem* OSS = IOnlineSubsystem::Get(FName("Steam"));
    if (!OSS)
    {
        NEXUS_ERROR("[SteamOnline] IOnlineSubsystem::Get(Steam)  returned null");
        return false;
    }
    SessionInterface = OSS->GetSessionInterface();
    return SessionInterface.IsValid();
}

TSharedPtr<const FUniqueNetId> FNexusSteamOnline::GetLocalPlayerId() const
{
    UWorld* World = GetWorld();
    if (!World) return nullptr;
    const ULocalPlayer* LocalPlayer = World->GetFirstLocalPlayerFromController();
    if (!LocalPlayer) return nullptr;
    const FUniqueNetIdRepl UniqueId = LocalPlayer->GetPreferredUniqueNetId();
    return UniqueId.IsValid() ? UniqueId.GetUniqueNetId() : nullptr;
}

FString FNexusSteamOnline::GetCurrentSessionId() const
{
    if (!SessionInterface.IsValid()) return FString();
    FNamedOnlineSession* Session = SessionInterface->GetNamedSession(NAME_GameSession);
    if (!Session || !Session->SessionInfo.IsValid()) return FString();
    return Session->GetSessionIdStr();
}

bool FNexusSteamOnline::GetSearchResultById(
    const FString& SessionId, FOnlineSessionSearchResult& OutResult) const
{
    if (!LastSessionSearch.IsValid()) return false;
    for (const FOnlineSessionSearchResult& Result : LastSessionSearch->SearchResults)
        if (Result.GetSessionIdStr() == SessionId)
        {
            OutResult = Result;
            return true;
        }
    return false;
}
