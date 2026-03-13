// Copyright NexusMultiplayer. All Rights Reserved.

#include "Core/NexusMultiplayerSubsystem.h"
#include "Core/NexusLog.h"
#include "Core/NexusMultiplayerSettings.h"
#include "Core/NexusTypes.h"

// Online layers
#include "Online/Steam/NexusSteamOnline.h"
#include "Online/Null/NexusNullOnline.h"

// Backend layers
#include "Backend/Platform/NexusPlatformBackend.h"
#include "Backend/Nakama/NexusNakamaBackend.h"

// Match code layers
#include "MatchCode/NexusSteamMatchCode.h"
#include "MatchCode/NexusNakamaMatchCode.h"
#include "MatchCode/NexusNullMatchCode.h"

// UE
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "OnlineSessionSettings.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Online/EOS/NexusEOSOnline.h"


// ─── Init / Deinit ────────────────────────────────────────────────────────────

void UNexusMultiplayerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    SelectLayers();
    BindLayerDelegates();

    IOnlineSubsystem* EOSOSS = IOnlineSubsystem::Get(FName("EOS"));
    if (EOSOSS)
    {
        IOnlineIdentityPtr Identity = EOSOSS->GetIdentityInterface();
        if (Identity.IsValid())
        {
            if (Identity->GetLoginStatus(0) == ELoginStatus::LoggedIn)
            {
                NEXUS_LOG("[Subsystem] EOS already logged in — starting Nakama directly");
                if (BackendLayer.IsValid()) BackendLayer->Login();
                return;
            }

            EOSLoginHandle = Identity->AddOnLoginCompleteDelegate_Handle(0,
                FOnLoginCompleteDelegate::CreateUObject(this,
                    &UNexusMultiplayerSubsystem::HandleEOSLoginComplete));
            Identity->Login(0, FOnlineAccountCredentials("accountportal", "", ""));
            return;
        }
    }

    if (BackendLayer.IsValid()) BackendLayer->Login();
}

void UNexusMultiplayerSubsystem::HandleEOSLoginComplete(
    int32 LocalUserNum, bool bWasSuccessful,
    const FUniqueNetId& UserId, const FString& Error)
{
    IOnlineSubsystem* EOSOSS = IOnlineSubsystem::Get(FName("EOS"));
    if (EOSOSS)
    {
        IOnlineIdentityPtr Identity = EOSOSS->GetIdentityInterface();
        if (Identity.IsValid())
            Identity->ClearOnLoginCompleteDelegate_Handle(0, EOSLoginHandle);
    }

    if (!bWasSuccessful)
        NEXUS_WARN("[Subsystem] EOS login failed (%s) — Nakama will use Device auth", *Error);

    if (BackendLayer.IsValid()) BackendLayer->Login();
}

void UNexusMultiplayerSubsystem::Deinitialize()
{
    NEXUS_LOG("[Subsystem] Deinitializing");

    IOnlineSubsystem* EOSOSS = IOnlineSubsystem::Get(FName("EOS"));
    if (EOSOSS)
    {
        IOnlineIdentityPtr Identity = EOSOSS->GetIdentityInterface();
        if (Identity.IsValid())
            Identity->ClearOnLoginCompleteDelegate_Handle(0, EOSLoginHandle);
    }
    
    // Clear all TFunction callbacks before layers are destroyed
    if (OnlineLayer.IsValid())
    {
        OnlineLayer->OnCreateComplete       = nullptr;
        OnlineLayer->OnFindComplete         = nullptr;
        OnlineLayer->OnJoinComplete         = nullptr;
        OnlineLayer->OnDestroyComplete      = nullptr;
        OnlineLayer->OnAttributeSessionFound = nullptr;
    }

    if (BackendLayer.IsValid())
    {
        BackendLayer->OnAccountLoaded       = nullptr;
        BackendLayer->OnDisplayNameUpdated  = nullptr;
        BackendLayer->OnLoginError          = nullptr;
    }

    if (MatchCodeLayer.IsValid())
    {
        MatchCodeLayer->OnCodeStored   = nullptr;
        MatchCodeLayer->OnSessionFound = nullptr;
        MatchCodeLayer->OnCodeError    = nullptr;
    }

    OnlineLayer.Reset();
    BackendLayer.Reset();
    MatchCodeLayer.Reset();

    Super::Deinitialize();
}

// ─── Layer Selection ──────────────────────────────────────────────────────────

// Returns the current session ID from whichever platform online layer is active.
// Works for both Steam and EOS — both expose GetCurrentSessionId().
static FString GetPlatformSessionId(INexusOnlineInterface* Layer)
{
    if (!Layer) return FString();

    if (Layer->GetMode() == ENexusOnlineMode::Steam)
        return static_cast<FNexusSteamOnline*>(Layer)->GetCurrentSessionId();

    if (Layer->GetMode() == ENexusOnlineMode::EOS)
        return static_cast<FNexusEOSOnline*>(Layer)->GetCurrentSessionId();

    return FString();
}

void UNexusMultiplayerSubsystem::SelectLayers()
{
    const UNexusMultiplayerSettings* S = UNexusMultiplayerSettings::Get();
    UWorld* World = GetWorld();

    // ── Online layer ──────────────────────────────────────────────────────────

    switch (S->OnlineMode)
    {
    case ENexusOnlineMode::Steam:
        NEXUS_LOG("[Subsystem] Online layer: FNexusSteamOnline");
        OnlineLayer = MakeShared<FNexusSteamOnline>(GetGameInstance());

        break;

    case ENexusOnlineMode::EOS:
        NEXUS_LOG("[Subsystem] Online layer: FNexusEOSOnline");
        OnlineLayer = MakeShared<FNexusEOSOnline>(GetGameInstance());
        break;

    case ENexusOnlineMode::Null:
    default:
        NEXUS_LOG("[Subsystem] Online layer: FNexusNullOnline (LAN/Dev)");
        OnlineLayer = MakeShared<FNexusNullOnline>(GetGameInstance());
        break;
    }

    // ── Backend layer ─────────────────────────────────────────────────────────

    switch (S->BackendMode)
    {
    case ENexusBackendMode::Nakama:
        NEXUS_LOG("[Subsystem] Backend layer: FNexusNakamaBackend → %s:%d",
            *S->NakamaHost, S->NakamaPort);
        BackendLayer = MakeShared<FNexusNakamaBackend>(
    S->NakamaHost, S->NakamaPort, S->NakamaServerKey,
    S->bNakamaUseSSL, GetGameInstance());
        break;

    case ENexusBackendMode::PlatformOnly:
    default:
        NEXUS_LOG("[Subsystem] Backend layer: FNexusPlatformBackend");
        BackendLayer = MakeShared<FNexusPlatformBackend>(OnlineLayer);
        break;
    }

    // ── Match code layer ──────────────────────────────────────────────────────
    // Selection priority: Nakama > Steam > Null

    if (S->BackendMode == ENexusBackendMode::Nakama)
    {
        auto* NakamaBackend = static_cast<FNexusNakamaBackend*>(BackendLayer.Get());
        NEXUS_LOG("[Subsystem] MatchCode layer: FNexusNakamaMatchCode");
        MatchCodeLayer = MakeShared<FNexusNakamaMatchCode>(
            NakamaBackend->GetClient(), NakamaBackend->GetSession());
    }
   else if (S->OnlineMode == ENexusOnlineMode::Steam
          || S->OnlineMode == ENexusOnlineMode::EOS) 
    {
        NEXUS_LOG("[Subsystem] MatchCode layer: FNexusSteamMatchCode");
        MatchCodeLayer = MakeShared<FNexusSteamMatchCode>(OnlineLayer);
    }
    else
    {
        NEXUS_LOG("[Subsystem] MatchCode layer: FNexusNullMatchCode (in-memory)");
        MatchCodeLayer = MakeShared<FNexusNullMatchCode>();
    }

    NEXUS_SUCCESS("[Subsystem] All layers selected");
}

void UNexusMultiplayerSubsystem::BindLayerDelegates()
{
    // ── Online → subsystem ────────────────────────────────────────────────────

    if (OnlineLayer.IsValid())
    {
        OnlineLayer->OnCreateComplete = [this](bool bSuccess)
        {
            HandleCreateComplete(bSuccess);
        };
        OnlineLayer->OnFindComplete = [this](
            const TArray<FOnlineSessionSearchResult>& Results, bool bSuccess)
        {
            HandleFindComplete(Results, bSuccess);
        };
        OnlineLayer->OnJoinComplete = [this](bool bSuccess)
        {
            HandleJoinComplete(bSuccess);
        };
        OnlineLayer->OnDestroyComplete = [this](bool bSuccess)
        {
            HandleDestroyComplete(bSuccess);
        };
        // OnAttributeSessionFound is wired inside FNexusSteamMatchCode directly
        // so we only need it here for the Nakama flow (JoinSessionById path)
        OnlineLayer->OnAttributeSessionFound = [this](const FString& SessionId)
        {
            // This fires when MatchCode found a session — now join it
            HandleSessionFound(SessionId);
        };
    }

    // ── Backend → subsystem ───────────────────────────────────────────────────

    if (BackendLayer.IsValid())
    {
        BackendLayer->OnAccountLoaded = [this]()
        {
            HandleAccountLoaded();
        };
        BackendLayer->OnDisplayNameUpdated = [this](const FString& Name)
        {
            HandleDisplayNameUpdated(Name);
        };
        BackendLayer->OnLoginError = [this](const FString& Error)
        {
            HandleLoginError(Error);
        };

        BackendLayer->OnScoreSubmitted = [this](const FString& Id)
        {
            HandleScoreSubmitted(Id);
        };
        BackendLayer->OnTopScoresFetched = [this](const FString& Id, TArray<FNexusLeaderboardEntry> Entries)
        {
            HandleTopScoresFetched(Id, MoveTemp(Entries));
        };
        BackendLayer->OnLeaderboardError = [this](const FString& Err)
        {
            HandleLeaderboardError(Err);
        };
    }

    // ── MatchCode → subsystem ─────────────────────────────────────────────────

    if (MatchCodeLayer.IsValid())
    {
        MatchCodeLayer->OnCodeStored = [this](const FString& Code)
        {
            HandleCodeStored(Code);
        };
        MatchCodeLayer->OnSessionFound = [this](const FString& SessionId)
        {
            HandleSessionFound(SessionId);
        };
        MatchCodeLayer->OnCodeError = [this](const FString& Error)
        {
            HandleCodeError(Error);
        };
    }

    NEXUS_LOG("[Subsystem] All delegates bound");
}

// ─── Blueprint API ────────────────────────────────────────────────────────────

void UNexusMultiplayerSubsystem::HostSession(FNexusSessionConfig Config)
{
    if (!OnlineLayer.IsValid())
    {
        NEXUS_ERROR("[Subsystem] HostSession: no online layer");
        OnSessionFailed.Broadcast(TEXT("Online layer not initialized"));
        return;
    }
    OnlineLayer->SetLANMode(NetworkMode == ENexusNetworkMode::LAN);

    NEXUS_LOG("[Subsystem] HostSession called. Players=%d | Map=%s | Private=%s | Type=%s",
        Config.MaxPlayers,
        *Config.LobbyMapPath,
        Config.bPrivate ? TEXT("YES") : TEXT("NO"),
        *Config.MatchType);

    LastSessionConfig = Config;
    bIsHost           = true;
    bIsPrivate        = Config.bPrivate;
    ActiveMatchCode.Empty();
    
    // HostSession():
    SetSessionState(ENexusSessionState::Creating); 
    OnlineLayer->CreateSession(Config);
}

void UNexusMultiplayerSubsystem::FindSessions(int32 MaxResults)
{
    if (!OnlineLayer.IsValid())
    {
        NEXUS_ERROR("[Subsystem] FindSessions: no online layer");
        OnSessionFailed.Broadcast(TEXT("Online layer not initialized"));
        return;
    }

    OnlineLayer->SetLANMode(NetworkMode == ENexusNetworkMode::LAN);

    NEXUS_LOG("[Subsystem] FindSessions called. MaxResults=%d", MaxResults);
    
    // FindSessions():
    SetSessionState(ENexusSessionState::Searching);
    OnlineLayer->FindSessions(MaxResults);
}

void UNexusMultiplayerSubsystem::JoinByCode(const FString& Code)
{
    if (Code.IsEmpty())
    {
        NEXUS_ERROR("[Subsystem] JoinByCode: code is empty");
        OnSessionFailed.Broadcast(TEXT("Match code is empty"));
        return;
    }

    if (!MatchCodeLayer.IsValid())
    {
        NEXUS_ERROR("[Subsystem] JoinByCode: no match code layer");
        OnSessionFailed.Broadcast(TEXT("Match code layer not initialized"));
        return;
    }

    // Normalize input — uppercase, trimmed
    const FString NormalizedCode = Code.TrimStartAndEnd().ToUpper();

    NEXUS_LOG("[Subsystem] JoinByCode: looking up code '%s'", *NormalizedCode);

    MatchCodeLayer->LookupCode(NormalizedCode);
    // Flow continues in HandleSessionFound (success) or HandleCodeError (failure)
}

void UNexusMultiplayerSubsystem::JoinSessionFromRow(const FNexusServerRow& Row)
{
    if (!OnlineLayer.IsValid())
    {
        NEXUS_ERROR("[Subsystem] JoinSessionFromRow: no online layer");
        OnSessionFailed.Broadcast(TEXT("Online layer not initialized"));
        return;
    }
 
    // LAN direct join — bypass OSS, ClientTravel to host IP
    if (!Row.HostAddress.IsEmpty())
    {
        NEXUS_LOG("[Subsystem] JoinSessionFromRow: LAN join → %s", *Row.HostAddress);
 
        bIsHost = false;
 
        UWorld* W = GetWorld();
        APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;
        if (!PC)
        {
            NEXUS_ERROR("[Subsystem] JoinSessionFromRow: no PlayerController");
            OnSessionFailed.Broadcast(TEXT("No PlayerController for ClientTravel"));
            return;
        }
 
        OnSessionJoined.Broadcast(true);

        // JoinSessionFromRow():
        SetSessionState(ENexusSessionState::Joining);
        PC->ClientTravel(Row.HostAddress, TRAVEL_Absolute);
        return;
    }
 
    // OSS join (Steam etc.)
    NEXUS_LOG("[Subsystem] JoinSessionFromRow: OSS join — SessionId='%s' Host='%s'",
        *Row.SessionId, *Row.HostName);
 
    const FOnlineSessionSearchResult* Result = FindResultBySessionId(Row.SessionId);
    if (!Result)
    {
        NEXUS_ERROR("[Subsystem] JoinSessionFromRow: session '%s' not in cache — "
            "did FindSessions run first?", *Row.SessionId);
        OnSessionFailed.Broadcast(TEXT("Session not found — please refresh the server list"));
        return;
    }
 
    bIsHost = false;

    // JoinSessionFromRow():
    SetSessionState(ENexusSessionState::Joining);
    OnlineLayer->JoinSession(*Result);
}

void UNexusMultiplayerSubsystem::SetNetworkMode(ENexusNetworkMode NewMode)
{
    NetworkMode = NewMode;
    NEXUS_LOG("[Subsystem] NetworkMode set to: %s",
        NewMode == ENexusNetworkMode::LAN ? TEXT("LAN") : TEXT("Internet"));
}

void UNexusMultiplayerSubsystem::LeaveSession()
{
    if (!OnlineLayer.IsValid())
    {
        NEXUS_WARN("[Subsystem] LeaveSession: no online layer — nothing to leave");
        return;
    }

    NEXUS_LOG("[Subsystem] LeaveSession called. IsHost=%s | Code='%s'",
        bIsHost ? TEXT("YES") : TEXT("NO"), *ActiveMatchCode);

    // Host cleans up the match code before destroying the session
    if (bIsHost && !ActiveMatchCode.IsEmpty() && MatchCodeLayer.IsValid())
    {
        NEXUS_LOG("[Subsystem] Deleting match code '%s' before leaving", *ActiveMatchCode);
        MatchCodeLayer->DeleteCode(ActiveMatchCode);
    }

    ActiveMatchCode.Empty();
    bIsHost    = false;
    bIsPrivate = false;

    OnlineLayer->DestroySession();
}

// ─── Online Callbacks ─────────────────────────────────────────────────────────

void UNexusMultiplayerSubsystem::HandleCreateComplete(bool bSuccess)
{
    if (!bSuccess)
    {
        NEXUS_ERROR("[Subsystem] Session create failed");
        bIsHost = false;
        OnSessionCreated.Broadcast(false);
        OnSessionFailed.Broadcast(TEXT("Failed to create session"));
        return;
    }

    NEXUS_SUCCESS("[Subsystem] Session created. Private=%s",
        bIsPrivate ? TEXT("YES") : TEXT("NO"));

    OnSessionCreated.Broadcast(true);

    // Travel to lobby map as listen server
    UWorld* World = GetWorld();
    if (!World)
    {
        NEXUS_ERROR("[Subsystem] HandleCreateComplete: World is null — cannot travel");
        return;
    }

    const FString TravelURL = FString::Printf(TEXT("%s?listen"),
        *LastSessionConfig.LobbyMapPath);

    NEXUS_LOG("[Subsystem] Server traveling to: %s", *TravelURL);
    World->ServerTravel(TravelURL);

    // If private session, generate and store a match code
    if (MatchCodeLayer.IsValid())
    {
        const FString Code = MatchCodeLayer->GenerateCode();
        NEXUS_LOG("[Subsystem] Private session — generated code '%s'. Storing...", *Code);

        // For Steam: session ID is implicit (we're the active host)
        // For Nakama: subsystem doesn't have the session ID here — SteamOnline
        //             can expose it, so we fetch it if available
        FString SessionId = GetPlatformSessionId(OnlineLayer.Get());
       
        MatchCodeLayer->StoreCode(Code, SessionId);
        // Flow continues in HandleCodeStored
    }
}

void UNexusMultiplayerSubsystem::HandleFindComplete(
    const TArray<FOnlineSessionSearchResult>& Results, bool bSuccess)
{
    TArray<FNexusServerRow> Rows;
 
    // Custom discovery path (e.g. FNexusNullOnline UDP)
    if (OnlineLayer && OnlineLayer->UsesCustomDiscovery())
    {
        OnlineLayer->GetDiscoveredRows(Rows);
 
        NEXUS_LOG("[Subsystem] FindSessions (custom): %d session(s) discovered", Rows.Num());
 
        if (Rows.IsEmpty())
        {
            NEXUS_WARN("[Subsystem] FindSessions: no LAN sessions found");
            OnSessionListReady.Broadcast({});
        }
        else
        {
            OnSessionListReady.Broadcast(Rows);
        }
        return;
    }
 
    // OSS path (Steam etc.)
    CachedSearchResults = Results;
 
    if (!bSuccess || Results.Num() == 0)
    {
        NEXUS_WARN("[Subsystem] FindSessions: no results or failed. Count=%d Success=%s",
            Results.Num(), bSuccess ? TEXT("YES") : TEXT("NO"));
        OnSessionListReady.Broadcast({});
        return;
    }
 
    NEXUS_SUCCESS("[Subsystem] FindSessions complete: %d session(s) found", Results.Num());
 
    Rows.Reserve(Results.Num());
    for (const FOnlineSessionSearchResult& Result : Results)
        Rows.Add(MakeServerRow(Result));
 
    OnSessionListReady.Broadcast(Rows);
}

void UNexusMultiplayerSubsystem::HandleJoinComplete(bool bSuccess)
{
    if (!bSuccess)
    {
        NEXUS_ERROR("[Subsystem] Join failed");
        OnSessionJoined.Broadcast(false);
        OnSessionFailed.Broadcast(TEXT("Failed to join session"));
        return;
    }

    NEXUS_SUCCESS("[Subsystem] Join successful — traveling to host");
    OnSessionJoined.Broadcast(true);
    TravelToSession();
}

void UNexusMultiplayerSubsystem::HandleDestroyComplete(bool bSuccess)
{
    // If we intentionally traveled to game map, ignore destroy callbacks
    // (UE destroys the session internally during ServerTravel)
    NEXUS_LOG("[Subsystem] Session destroyed. Success=%s | IsHost=%s",
        bSuccess ? TEXT("YES") : TEXT("NO"),
        bIsHost ? TEXT("YES") : TEXT("NO"));
}

// ─── Match Code Callbacks ─────────────────────────────────────────────────────

void UNexusMultiplayerSubsystem::HandleCodeStored(FString Code)
{
    ActiveMatchCode = Code;

    NEXUS_SUCCESS("[Subsystem] Match code ready: '%s'", *Code);
    OnMatchCodeReady.Broadcast(Code);
}

void UNexusMultiplayerSubsystem::HandleSessionFound(FString SessionId)
{
    if (SessionId.IsEmpty())
    {
        NEXUS_ERROR("[Subsystem] HandleSessionFound: SessionId is empty — join aborted");
        OnSessionFailed.Broadcast(TEXT("Invalid session ID returned from code lookup"));
        return;
    }

    NEXUS_LOG("[Subsystem] Code resolved to SessionId='%s' — attempting join", *SessionId);

    if (!OnlineLayer.IsValid())
    {
        NEXUS_ERROR("[Subsystem] HandleSessionFound: no online layer");
        OnSessionFailed.Broadcast(TEXT("Online layer not available"));
        return;
    }

    // Check cache first — avoids a second network search if session was already found
    const FOnlineSessionSearchResult* Cached = FindResultBySessionId(SessionId);
    if (Cached)
    {
        NEXUS_LOG("[Subsystem] Session found in cache — joining directly");
        bIsHost = false;

        // JoinSessionFromRow():
        SetSessionState(ENexusSessionState::Joining);
        OnlineLayer->JoinSession(*Cached);
    }
    else
    {
        // Not in cache — tell the online layer to find it by ID
        // (triggers another FindSessions internally, filtered by ID)
        NEXUS_LOG("[Subsystem] Session not in cache — searching by ID");
        OnlineLayer->JoinSessionById(SessionId);
    }
}

void UNexusMultiplayerSubsystem::HandleCodeError(FString Error)
{
    NEXUS_ERROR("[Subsystem] Match code error: %s", *Error);

    // If Nakama session expired but Steam is available, fall back to Steam match code
    const UNexusMultiplayerSettings* S = UNexusMultiplayerSettings::Get();
    if (S->BackendMode == ENexusBackendMode::Nakama
    && (S->OnlineMode == ENexusOnlineMode::Steam
     || S->OnlineMode == ENexusOnlineMode::EOS)
    && bIsHost && ActiveMatchCode.IsEmpty())
    {
        NEXUS_WARN("[Subsystem] Nakama match code failed — falling back to SteamMatchCode");
        MatchCodeLayer = MakeShared<FNexusSteamMatchCode>(OnlineLayer);
        MatchCodeLayer->OnCodeStored  = [this](const FString& Code) { HandleCodeStored(Code); };
        MatchCodeLayer->OnSessionFound= [this](const FString& Id)   { HandleSessionFound(Id); };
        MatchCodeLayer->OnCodeError   = [this](const FString& Err)  { HandleCodeError(Err); };

        // Re-attempt with the Steam layer
        FString SessionId = GetPlatformSessionId(OnlineLayer.Get());
        const FString Code = MatchCodeLayer->GenerateCode();
        MatchCodeLayer->StoreCode(Code, SessionId);
        
        return;
    }

    OnSessionFailed.Broadcast(Error);
}

// ─── Backend Callbacks ────────────────────────────────────────────────────────

void UNexusMultiplayerSubsystem::HandleAccountLoaded()
{
    const FString Name = BackendLayer.IsValid()
        ? BackendLayer->GetDisplayName()
        : FString();

    NEXUS_SUCCESS("[Subsystem] Account loaded. DisplayName='%s'", *Name);

    // Update Nakama match code layer with fresh session after login
    if (BackendLayer->GetMode() == ENexusBackendMode::Nakama)
    {
        auto* NakamaBack = static_cast<FNexusNakamaBackend*>(BackendLayer.Get());
        auto* NakamaMatch = static_cast<FNexusNakamaMatchCode*>(MatchCodeLayer.Get());
        NakamaMatch->UpdateSession(NakamaBack->GetSession());
        NEXUS_LOG("[Subsystem] NakamaMatchCode session refreshed after login");
    }

    if (!Name.IsEmpty())
        OnDisplayNameReady.Broadcast(Name);

    if (BackendLayer->GetMode() == ENexusBackendMode::Nakama)
    {
        auto* NakamaBack = static_cast<FNexusNakamaBackend*>(BackendLayer.Get());

        MatchCodeLayer = MakeShared<FNexusNakamaMatchCode>(
            NakamaBack->GetClient(),
            NakamaBack->GetSession());

        MatchCodeLayer->OnCodeStored = [this](const FString& Code)
        {
            HandleCodeStored(Code);
        };

        MatchCodeLayer->OnSessionFound = [this](const FString& SessionId)
        {
            HandleSessionFound(SessionId);
        };

        MatchCodeLayer->OnCodeError = [this](const FString& Error)
        {
            HandleCodeError(Error);
        };
    }
}

void UNexusMultiplayerSubsystem::HandleDisplayNameUpdated(FString Name)
{
    NEXUS_SUCCESS("[Subsystem] Display name updated: '%s'", *Name);
    OnDisplayNameReady.Broadcast(Name);
}

void UNexusMultiplayerSubsystem::HandleLoginError(FString Error)
{
    // Backend login failure is non-fatal in PlatformOnly mode
    // In Nakama mode it means server is unreachable — warn but let the game run
    NEXUS_ERROR("[Subsystem] Backend login error: %s", *Error);
    OnSessionFailed.Broadcast(FString::Printf(TEXT("Backend error: %s"), *Error));
}

// ─── Queries ──────────────────────────────────────────────────────────────────

FString UNexusMultiplayerSubsystem::GetLocalDisplayName() const
{
    if (BackendLayer.IsValid() && BackendLayer->IsLoggedIn())
        return BackendLayer->GetDisplayName();

    if (OnlineLayer.IsValid())
        return OnlineLayer->GetDisplayName();

    return FPlatformProcess::UserName(false);
}

bool UNexusMultiplayerSubsystem::IsInSession() const
{
    return !GetPlatformSessionId(OnlineLayer.Get()).IsEmpty();
}

bool UNexusMultiplayerSubsystem::IsBackendConnected() const
{
    return BackendLayer.IsValid() && BackendLayer->IsLoggedIn();
}

// ─── Helpers ──────────────────────────────────────────────────────────────────

UWorld* UNexusMultiplayerSubsystem::GetWorld() const
{
    return GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
}

void UNexusMultiplayerSubsystem::TravelToSession()
{
    if (!OnlineLayer.IsValid()) return;

    UWorld* World = GetWorld();
    if (!World)
    {
        NEXUS_ERROR("[Subsystem] TravelToSession: World is null");
        return;
    }

    // Get the connect string from the session (Steam: steam IP, EOS: EOS addr)
    APlayerController* PC = World->GetFirstPlayerController();
    if (!PC)
    {
        NEXUS_ERROR("[Subsystem] TravelToSession: no PlayerController");
        return;
    }

    // OSS gives us the resolved connection string for the active session
    IOnlineSubsystem* OSS = IOnlineSubsystem::Get();
    if (!OSS) return;

    IOnlineSessionPtr Sessions = OSS->GetSessionInterface();
    if (!Sessions.IsValid()) return;

    FString TravelURL;
    if (!Sessions->GetResolvedConnectString(NAME_GameSession, TravelURL))
    {
        NEXUS_ERROR("[Subsystem] TravelToSession: GetResolvedConnectString failed");
        return;
    }

    NEXUS_LOG("[Subsystem] Client traveling to: %s", *TravelURL);

    // JoinSessionFromRow():
    SetSessionState(ENexusSessionState::Joining);
    PC->ClientTravel(TravelURL, TRAVEL_Absolute);
}

FNexusServerRow UNexusMultiplayerSubsystem::MakeServerRow(
    const FOnlineSessionSearchResult& Result) const
{
    FNexusServerRow Row;
    Row.SessionId = Result.GetSessionIdStr();

    Result.Session.SessionSettings.Get(FName("HostDisplayName"), Row.HostName);
    Result.Session.SessionSettings.Get(FName("MatchType"), Row.MatchType);

    Row.MaxPlayers     = Result.Session.SessionSettings.NumPublicConnections;
    Row.CurrentPlayers = Row.MaxPlayers - Result.Session.NumOpenPublicConnections;
    Row.PingMs         = Result.PingInMs;

    return Row;
}

const FOnlineSessionSearchResult* UNexusMultiplayerSubsystem::FindResultBySessionId(
    const FString& SessionId) const
{
    for (const FOnlineSessionSearchResult& Result : CachedSearchResults)
        if (Result.GetSessionIdStr() == SessionId)
            return &Result;
    return nullptr;
}

FString UNexusMultiplayerSubsystem::GetLocalUserId() const
{
    // Try backend user ID first (Nakama/Platform)
    if (BackendLayer)
    {
        const FString BackendId = BackendLayer->GetBackendUserId();
        if (!BackendId.IsEmpty())
            return BackendId;
    }

    // Fall back to online subsystem unique net ID
    IOnlineSubsystem* OSS = IOnlineSubsystem::Get();
    if (OSS)
    {
        IOnlineIdentityPtr Identity = OSS->GetIdentityInterface();
        if (Identity.IsValid())
        {
            TSharedPtr<const FUniqueNetId> Id =
                Identity->GetUniquePlayerId(0);
            if (Id.IsValid())
                return Id->ToString();
        }
    }

    // Last resort — machine login ID
    return FPlatformMisc::GetLoginId();
}

void UNexusMultiplayerSubsystem::SetSessionState(ENexusSessionState NewState)
{
    if (SessionState == NewState) return;
    const ENexusSessionState Old = SessionState;
    SessionState = NewState;
    NEXUS_LOG("[Subsystem] SessionState: %d → %d", (int32)Old, (int32)NewState);
    OnSessionStateChanged.Broadcast(Old, NewState);
}
 
void UNexusMultiplayerSubsystem::SubmitScore(const FString& LeaderboardId, int64 Score)
{
    if (!BackendLayer.IsValid())
    {
        NEXUS_ERROR("[Subsystem] SubmitScore: no backend layer");
        OnLeaderboardError.Broadcast(TEXT("Backend not initialized"));
        return;
    }
    if (!BackendLayer->IsLoggedIn())
    {
        NEXUS_ERROR("[Subsystem] SubmitScore: not logged in to backend");
        OnLeaderboardError.Broadcast(TEXT("Not logged in to backend"));
        return;
    }
    BackendLayer->SubmitScore(LeaderboardId, Score);
}
 
void UNexusMultiplayerSubsystem::FetchTopScores(const FString& LeaderboardId, int32 Limit)
{
    if (!BackendLayer.IsValid())
    {
        NEXUS_ERROR("[Subsystem] FetchTopScores: no backend layer");
        OnLeaderboardError.Broadcast(TEXT("Backend not initialized"));
        return;
    }
    if (!BackendLayer->IsLoggedIn())
    {
        NEXUS_ERROR("[Subsystem] FetchTopScores: not logged in to backend");
        OnLeaderboardError.Broadcast(TEXT("Not logged in to backend"));
        return;
    }
    BackendLayer->FetchTopScores(LeaderboardId, FMath::Clamp(Limit, 1, 100));
}
 
void UNexusMultiplayerSubsystem::HandleScoreSubmitted(FString LeaderboardId)
{
    NEXUS_SUCCESS("[Subsystem] Score submitted to board: %s", *LeaderboardId);
    OnScoreSubmitted.Broadcast(LeaderboardId);
}
 
void UNexusMultiplayerSubsystem::HandleTopScoresFetched(
    FString LeaderboardId, TArray<FNexusLeaderboardEntry> Entries)
{
    NEXUS_SUCCESS("[Subsystem] Top scores fetched. Board: %s | Count: %d",
        *LeaderboardId, Entries.Num());
    OnTopScoresFetched.Broadcast(LeaderboardId, Entries);
}
 
void UNexusMultiplayerSubsystem::HandleLeaderboardError(FString Error)
{
    NEXUS_ERROR("[Subsystem] Leaderboard error: %s", *Error);
    OnLeaderboardError.Broadcast(Error);
}