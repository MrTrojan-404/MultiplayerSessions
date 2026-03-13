// Copyright NexusMultiplayer. All Rights Reserved.

#include "Backend/Nakama/NexusNakamaBackend.h"
#include "Core/NexusLog.h"
#include "NakamaClient.h"
#include "NakamaSession.h"
#include "OnlineSubsystemUtils.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Kismet/GameplayStatics.h"
#include "Backend/Nakama/SaveGame/NexusNakamaSession.h"

// ── Constructor ───────────────────────────────────────────────────────────────

FNexusNakamaBackend::FNexusNakamaBackend(
    const FString& InHost, int32 InPort,
    const FString& ServerKey, bool bUseSSL,
    UGameInstance* InGameInstance)
    : Host(InHost)
      , Port(InPort)
      , GameInstance(InGameInstance)
{
    NEXUS_LOG("[Nakama] Backend created. Host: %s:%d | SSL: %s",
        *Host, Port, bUseSSL ? TEXT("YES") : TEXT("NO"));

    NakamaClient = UNakamaClient::CreateDefaultClient(
        ServerKey, Host, Port, bUseSSL, /*bEnableDebug=*/true);

    if (NakamaClient)
        NakamaClient->AddToRoot();
    
    if (!NakamaClient)
        NEXUS_ERROR("[Nakama] Failed to create NakamaClient — check host/port/key");

    Proxy = NewObject<UNexusNakamaBackendProxy>(GetTransientPackage());
    Proxy->Owner = this;
    Proxy->AddToRoot(); // prevent GC from destroying it prematurely
}

FNexusNakamaBackend::~FNexusNakamaBackend()
{
    if (Proxy)
    {
        Proxy->HeldSession = nullptr;  // release session ref
        Proxy->Owner = nullptr;
        Proxy->RemoveFromRoot();
        Proxy = nullptr;
    }
}

// ── Login — three-tier priority chain ────────────────────────────────────────
//
//  Tier 1 — EOS Connect   (free, cross-platform, stable identity)
//  Tier 2 — Steam         (requires App ID + Nakama Web API key)
//  Tier 3 — Device        (anonymous GUID, persistent per-install)
//
//  Each tier falls through automatically via HandleAuthError.
// ─────────────────────────────────────────────────────────────────────────────

void FNexusNakamaBackend::Login()
{
    NEXUS_LOG("[Nakama] Login called");

    if (!NakamaClient)
    {
        NEXUS_ERROR("[Nakama] Login failed — NakamaClient is null");
        if (OnLoginError) OnLoginError(TEXT("NakamaClient not initialized"));
        return;
    }

    if (TryRestoreSession())
    {
        NEXUS_SUCCESS("[Nakama] Session restored — skipping full auth");
        return;
    }

    NEXUS_LOG("[Nakama] No valid saved session — starting fresh auth");

    // Tier 1: EOS Connect
    if (!GetEOSConnectId().IsEmpty())
    {
        NEXUS_LOG("[Nakama] EOS Connect available — Tier 1");
        LoginWithEOSConnect();
        return;
    }

    // Tier 2: Steam
    IOnlineSubsystem* OSS = IOnlineSubsystem::Get();
    if (OSS && OSS->GetSubsystemName() == FName("STEAM"))
    {
        NEXUS_LOG("[Nakama] Steam detected — Tier 2");
        LoginWithSteam();
        return;
    }

    // Tier 3: Device
    NEXUS_WARN("[Nakama] No platform auth — Device fallback (Tier 3)");
    LoginWithDevice();
}

// ── Tier 1 — EOS Connect ─────────────────────────────────────────────────────

void FNexusNakamaBackend::LoginWithEOSConnect()
{
    const FString EOSId = GetEOSConnectId();
    if (EOSId.IsEmpty())
    {
        NEXUS_WARN("[Nakama] EOS ID unavailable — falling to Tier 2");
        LoginWithSteam();
        return;
    }

    // Prefix "eos_" so Nakama can distinguish these accounts from raw device GUIDs.
    // Uses AuthenticateDevice — no server-side hook needed.
    // EOS ProductUserId is stable per-user per-game — survives reinstalls.
    const FString PrefixedId = FString::Printf(TEXT("eos_%s"), *EOSId);

    NEXUS_LOG("[Nakama] EOS Connect auth. ID: %s...", *PrefixedId.Left(16));

    ActiveProvider = ENexusNakamaAuthProvider::EOSConnect;

    FOnAuthUpdate Success;
    Success.AddDynamic(Proxy, &UNexusNakamaBackendProxy::OnAuthSuccess);
    FOnError Error;
    Error.AddDynamic(Proxy, &UNexusNakamaBackendProxy::OnAuthError);

    NakamaClient->AuthenticateDevice(PrefixedId, TEXT(""), true, {}, Success, Error);
}

// ── Tier 2 — Steam ───────────────────────────────────────────────────────────

void FNexusNakamaBackend::LoginWithSteam()
{
    const FString Ticket = GetSteamAuthTicket();
    if (Ticket.IsEmpty())
    {
        NEXUS_WARN("[Nakama] Steam ticket empty — falling to Tier 3");
        LoginWithDevice();
        return;
    }

    NEXUS_LOG("[Nakama] Steam auth. Ticket length: %d", Ticket.Len());

    ActiveProvider = ENexusNakamaAuthProvider::Steam;

    FOnAuthUpdate Success;
    Success.AddDynamic(Proxy, &UNexusNakamaBackendProxy::OnAuthSuccess);
    FOnError Error;
    Error.AddDynamic(Proxy, &UNexusNakamaBackendProxy::OnAuthError);

    NakamaClient->AuthenticateSteam(Ticket, TEXT(""), true, true, {}, Success, Error);
}

// ── Tier 3 — Device ──────────────────────────────────────────────────────────

void FNexusNakamaBackend::LoginWithDevice()
{
    const FString DeviceId = GetDeviceId();

    NEXUS_LOG("[Nakama] Device auth. ID: %s...", *DeviceId.Left(8));

    ActiveProvider = ENexusNakamaAuthProvider::Device;

    FOnAuthUpdate Success;
    Success.AddDynamic(Proxy, &UNexusNakamaBackendProxy::OnAuthSuccess);
    FOnError Error;
    Error.AddDynamic(Proxy, &UNexusNakamaBackendProxy::OnAuthError);

    NakamaClient->AuthenticateDevice(DeviceId, TEXT(""), true, {}, Success, Error);
}

// ── Auth Callbacks ────────────────────────────────────────────────────────────

void FNexusNakamaBackend::HandleAuthSuccess(UNakamaSession* Session)
{
    UserSession = Session;

    if (Proxy)
        Proxy->HeldSession = Session;
    
    const TCHAR* ProviderName =
        ActiveProvider == ENexusNakamaAuthProvider::EOSConnect ? TEXT("EOSConnect") :
        ActiveProvider == ENexusNakamaAuthProvider::Steam      ? TEXT("Steam")      :
                                                                  TEXT("Device");

    NEXUS_SUCCESS("[Nakama] Auth success. Provider: %s | User: %s",
        ProviderName, *Session->SessionData.Username);

    SaveSession();
    FetchAccount();
}

void FNexusNakamaBackend::HandleAuthError(const FNakamaError& Error)
{
    NEXUS_ERROR("[Nakama] Auth error (Provider: %d): %s",
        (int32)ActiveProvider, *Error.Message);

    if (ActiveProvider == ENexusNakamaAuthProvider::EOSConnect)
    {
        NEXUS_WARN("[Nakama] EOS Connect failed — falling to Steam (Tier 2)");
        LoginWithSteam();
        return;
    }

    if (ActiveProvider == ENexusNakamaAuthProvider::Steam)
    {
        NEXUS_WARN("[Nakama] Steam failed — falling to Device (Tier 3)");
        LoginWithDevice();
        return;
    }

    // Device auth failure = Nakama server unreachable
    NEXUS_ERROR("[Nakama] Device auth failed — is Nakama server running at %s:%d?",
        *Host, Port);

    if (OnLoginError) OnLoginError(Error.Message);
}

// ── Account ───────────────────────────────────────────────────────────────────

void FNexusNakamaBackend::FetchAccount()
{
    if (!UserSession || !NakamaClient)
    {
        NEXUS_ERROR("[Nakama] FetchAccount: no valid session or client");
        return;
    }

    FOnUserAccountInfo Success;
    Success.AddDynamic(Proxy, &UNexusNakamaBackendProxy::OnGetAccountSuccess);
    FOnError Error;
    Error.AddDynamic(Proxy, &UNexusNakamaBackendProxy::OnGetAccountError);

    NakamaClient->GetAccount(UserSession, Success, Error);
}

void FNexusNakamaBackend::HandleGetAccountSuccess(const FNakamaAccount& Account)
{
    if (UserSession)
        UserSession->SessionData.Username = Account.User.Username;

    NEXUS_SUCCESS("[Nakama] Account loaded. Username: %s | ID: %s",
        *Account.User.Username, *Account.User.Id);

    if (!HasMeaningfulName())
        TrySetBestAvailableName();

    if (OnAccountLoaded) OnAccountLoaded();
}

void FNexusNakamaBackend::HandleGetAccountError(const FNakamaError& Error)
{
    NEXUS_ERROR("[Nakama] FetchAccount failed: %s", *Error.Message);
    if (OnAccountLoaded) OnAccountLoaded(); // don't stall subsystem
}

// ── Display Name ──────────────────────────────────────────────────────────────

void FNexusNakamaBackend::SetDisplayName(const FString& NewName)
{
    if (!UserSession || !NakamaClient)
    {
        NEXUS_ERROR("[Nakama] SetDisplayName: not logged in");
        return;
    }

    PendingDisplayName = NewName;

    FOnUpdateAccount Success;
    Success.AddDynamic(Proxy, &UNexusNakamaBackendProxy::OnUpdateAccountSuccess);
    FOnError Error;
    Error.AddDynamic(Proxy, &UNexusNakamaBackendProxy::OnUpdateAccountError);

    NakamaClient->UpdateAccount(UserSession,
        NewName, TEXT(""), TEXT(""), TEXT(""), TEXT(""), TEXT(""),
        Success, Error);
}

void FNexusNakamaBackend::HandleSetDisplayNameSuccess()
{
    if (UserSession) UserSession->SessionData.Username = PendingDisplayName;
    NEXUS_SUCCESS("[Nakama] Display name updated: %s", *PendingDisplayName);
    if (OnDisplayNameUpdated) OnDisplayNameUpdated(PendingDisplayName);
}

void FNexusNakamaBackend::HandleSetDisplayNameError(const FNakamaError& Error)
{
    NEXUS_ERROR("[Nakama] SetDisplayName failed: %s", *Error.Message);
    if (OnDisplayNameUpdated) OnDisplayNameUpdated(GetDisplayName());
}

// ── Leaderboards ──────────────────────────────────────────────────────────────

void FNexusNakamaBackend::SubmitScore(const FString& LeaderboardId, int64 Score)
{
    if (!EnsureClientAndSession(TEXT("SubmitScore"))) return;
 
    NEXUS_LOG("[Nakama] SubmitScore: Board='%s' Score=%lld", *LeaderboardId, Score);
 
    PendingLeaderboardId = LeaderboardId;
 
    FOnWriteLeaderboardRecord Success;
    Success.AddDynamic(Proxy, &UNexusNakamaBackendProxy::OnWriteLeaderboardSuccess);
 
    FOnError Error;
    Error.AddDynamic(Proxy, &UNexusNakamaBackendProxy::OnWriteLeaderboardError);
 
    // Operator (Best/Set/Increment) is configured server-side in the Nakama console
    NakamaClient->WriteLeaderboardRecord(
        UserSession,
        LeaderboardId,
        Score,
        0,          // SubScore
        TEXT(""),   // Metadata
        Success,
        Error);
}

void FNexusNakamaBackend::HandleWriteLeaderboardSuccess(const FNakamaLeaderboardRecord& Record)
{
    NEXUS_SUCCESS("[Nakama] Score submitted. Board='%s' Rank=%d Score=%lld",
        *PendingLeaderboardId, Record.Rank, Record.Score);
 
    if (OnScoreSubmitted) OnScoreSubmitted(PendingLeaderboardId);
}

void FNexusNakamaBackend::HandleWriteLeaderboardError(const FNakamaError& Error)
{
    NEXUS_ERROR("[Nakama] SubmitScore failed: %s", *Error.Message);
    if (OnLeaderboardError) OnLeaderboardError(Error.Message);
}
 

void FNexusNakamaBackend::FetchTopScores(const FString& LeaderboardId, int32 Limit)
{
    if (!EnsureClientAndSession(TEXT("FetchTopScores"))) return;
 
    NEXUS_LOG("[Nakama] FetchTopScores: Board='%s' Limit=%d", *LeaderboardId, Limit);
 
    PendingLeaderboardId = LeaderboardId;
 
    FOnListLeaderboardRecords Success;
    Success.AddDynamic(Proxy, &UNexusNakamaBackendProxy::OnListLeaderboardSuccess);
 
    FOnError Error;
    Error.AddDynamic(Proxy, &UNexusNakamaBackendProxy::OnListLeaderboardError);
 
    // Empty OwnerIds = global top scores, not filtered to friends
    NakamaClient->ListLeaderboardRecords(
        UserSession,
        LeaderboardId,
        {},                                     // OwnerIds — empty = global leaderboard
        Limit,
        TEXT(""),                               // Cursor — empty = start from top
        ENakamaLeaderboardListBy::BY_SCORE,     // Order by score descending
        Success,
        Error);
}

void FNexusNakamaBackend::HandleListLeaderboardSuccess(
    const FNakamaLeaderboardRecordList& RecordList)
{
    NEXUS_SUCCESS("[Nakama] FetchTopScores: Board='%s' | %d records returned",
        *PendingLeaderboardId, RecordList.Records.Num());
 
    TArray<FNexusLeaderboardEntry> Entries;
    Entries.Reserve(RecordList.Records.Num());
 
    for (const FNakamaLeaderboardRecord& Record : RecordList.Records)
    {
        FNexusLeaderboardEntry Entry;
        Entry.UserId   = Record.OwnerId;
        Entry.Username = Record.Username;
        Entry.Score    = Record.Score;
        Entry.SubScore = Record.SubScore;
        Entry.Rank     = Record.Rank;
        Entries.Add(Entry);
    }
 
    if (OnTopScoresFetched) OnTopScoresFetched(PendingLeaderboardId, Entries);
}

void FNexusNakamaBackend::HandleListLeaderboardError(const FNakamaError& Error)
{
    NEXUS_ERROR("[Nakama] FetchTopScores failed: %s", *Error.Message);
    if (OnLeaderboardError) OnLeaderboardError(Error.Message);
}

bool FNexusNakamaBackend::EnsureClientAndSession(const FString& CallerName) const
{
    if (!NakamaClient)
    {
        NEXUS_ERROR("[Nakama] %s: NakamaClient is null", *CallerName);
        if (OnLeaderboardError)
            const_cast<FNexusNakamaBackend*>(this)
                ->OnLeaderboardError(TEXT("Nakama client not initialized"));
        return false;
    }
    if (!UserSession || UserSession->IsExpired())
    {
        NEXUS_ERROR("[Nakama] %s: Session null or expired", *CallerName);
        if (OnLeaderboardError)
            const_cast<FNexusNakamaBackend*>(this)
                ->OnLeaderboardError(TEXT("Not logged in to Nakama"));
        return false;
    }
    return true;
}

// ── Queries ───────────────────────────────────────────────────────────────────

bool FNexusNakamaBackend::IsLoggedIn() const
{
    return UserSession != nullptr && !UserSession->IsExpired();
}

FString FNexusNakamaBackend::GetDisplayName() const
{
    return UserSession ? UserSession->SessionData.Username : FString();
}

FString FNexusNakamaBackend::GetBackendUserId() const
{
    return UserSession ? UserSession->SessionData.UserId : FString();
}

// ── Helpers ───────────────────────────────────────────────────────────────────

FString FNexusNakamaBackend::GetEOSConnectId() const
{
    IOnlineSubsystem* EOSOSS = IOnlineSubsystem::Get(FName("EOS"));
    if (!EOSOSS)
    {
        NEXUS_LOG("[Nakama] EOS subsystem not found");
        return FString();
    }

    IOnlineIdentityPtr Identity = EOSOSS->GetIdentityInterface();
    if (!Identity.IsValid()) return FString();

    // Don't trigger an EOS login here — EOS auto-login runs at game startup.
    // If not logged in by the time Nakama Login() is called, skip to Steam.
    if (Identity->GetLoginStatus(0) == ELoginStatus::NotLoggedIn)
    {
        NEXUS_LOG("[Nakama] EOS not logged in — skipping EOS auth");
        return FString();
    }

    TSharedPtr<const FUniqueNetId> UserId = Identity->GetUniquePlayerId(0);
    if (!UserId.IsValid() || !UserId->IsValid()) return FString();

    // EOS ProductUserId — stable per-user per-game, survives reinstalls
    const FString IdStr = UserId->ToString();
    NEXUS_LOG("[Nakama] EOS Connect ID: %s...", *IdStr.Left(12));
    return IdStr;
}

FString FNexusNakamaBackend::GetSteamAuthTicket() const
{
    IOnlineSubsystem* OSS = IOnlineSubsystem::Get();
    if (!OSS) return FString();
    IOnlineIdentityPtr Identity = OSS->GetIdentityInterface();
    if (!Identity.IsValid()) return FString();
    return Identity->GetAuthToken(0);
}

FString FNexusNakamaBackend::GetDeviceId() const
{
    static FString CachedId;
    if (!CachedId.IsEmpty()) return CachedId;

    if (UGameplayStatics::DoesSaveGameExist(NEXUS_DEVICE_ID_SLOT, 0))
    {
        UNexusNakamaSession* Save = Cast<UNexusNakamaSession>(
    UGameplayStatics::LoadGameFromSlot(NEXUS_DEVICE_ID_SLOT, 0));
        if (Save && !Save->AuthToken.IsEmpty())
        {
            if (!Save->AuthToken.IsEmpty())
            {
                CachedId = Save->AuthToken;
                NEXUS_LOG("[Nakama] Device ID loaded: %s...", *CachedId.Left(8));
                return CachedId;
            }
        }
    }

    CachedId = FGuid::NewGuid().ToString();

    auto* Save = Cast<UNexusNakamaSession>(
        UGameplayStatics::CreateSaveGameObject(UNexusNakamaSession::StaticClass()));
    Save->AuthToken = CachedId;
    UGameplayStatics::SaveGameToSlot(Save, NEXUS_DEVICE_ID_SLOT, 0);

    NEXUS_LOG("[Nakama] New Device ID generated: %s...", *CachedId.Left(8));
    return CachedId;
}

bool FNexusNakamaBackend::HasMeaningfulName() const
{
    if (!UserSession) return false;
    const FString& Name = UserSession->SessionData.Username;
    if (Name.IsEmpty()) return false;
    if (Name.StartsWith(TEXT("steam_"))) return false;
    if (Name.StartsWith(TEXT("eos_")))   return false;

    // Device auth produces random 10-char alphanumeric strings
    if (Name.Len() == 10 && !Name.Contains(TEXT(" ")))
    {
        bool bAllAlphaNum = true;
        for (TCHAR c : Name)
            if (!FChar::IsAlpha(c) && !FChar::IsDigit(c))
            { bAllAlphaNum = false; break; }
        if (bAllAlphaNum) return false;
    }

    return true;
}

void FNexusNakamaBackend::TrySetBestAvailableName()
{
    // Priority 1 — EOS display name
    IOnlineSubsystem* EOSOSS = IOnlineSubsystem::Get(FName("EOS"));
    if (EOSOSS)
    {
        IOnlineIdentityPtr Identity = EOSOSS->GetIdentityInterface();
        if (Identity.IsValid() && Identity->GetLoginStatus(0) != ELoginStatus::NotLoggedIn)
        {
            const FString EOSName = Identity->GetPlayerNickname(0);
            if (!EOSName.IsEmpty())
            {
                NEXUS_LOG("[Nakama] Using EOS display name: %s", *EOSName);
                SetDisplayName(EOSName);
                return;
            }
        }
    }

    // Priority 2 — Steam display name
    IOnlineSubsystem* OSS = IOnlineSubsystem::Get();
    if (OSS)
    {
        IOnlineIdentityPtr Identity = OSS->GetIdentityInterface();
        if (Identity.IsValid())
        {
            const FString SteamName = Identity->GetPlayerNickname(0);
            if (!SteamName.IsEmpty())
            {
                NEXUS_LOG("[Nakama] Using Steam display name: %s", *SteamName);
                SetDisplayName(SteamName);
                return;
            }
        }
    }

    // Priority 3 — OS username
    const FString OSName = FPlatformProcess::UserName(false);
    if (!OSName.IsEmpty())
    {
        NEXUS_LOG("[Nakama] Using OS username: %s", *OSName);
        SetDisplayName(OSName);
        return;
    }

    // Priority 4 — trim auto-generated ID
    const FString Trimmed = FString::Printf(TEXT("Player_%s"),
        *GetDisplayName().Left(5).ToUpper());
    NEXUS_WARN("[Nakama] No better name — using: %s", *Trimmed);
    SetDisplayName(Trimmed);
}

// ── Session Persistence ───────────────────────────────────────────────────────

bool FNexusNakamaBackend::TryRestoreSession()
{
    if (!UGameplayStatics::DoesSaveGameExist(NEXUS_NAKAMA_SESSION_SLOT, 0))
        return false;

    UNexusNakamaSession* Save = Cast<UNexusNakamaSession>(
    UGameplayStatics::LoadGameFromSlot(NEXUS_NAKAMA_SESSION_SLOT, 0));

    if (!Save || Save->AuthToken.IsEmpty())
    {
        NEXUS_WARN("[Nakama] Saved session corrupt or empty");
        return false;
    }

    UserSession = UNakamaSession::RestoreSession(Save->AuthToken, Save->RefreshToken);

    if (!UserSession || UserSession->IsExpired())
    {
        NEXUS_WARN("[Nakama] Saved session expired — fresh auth required");
        UserSession = nullptr;
        return false;
    }

    if (Proxy)
        Proxy->HeldSession = UserSession;
    
    NEXUS_SUCCESS("[Nakama] Session restored. Fetching account...");
    FetchAccount();
    return true;
}

void FNexusNakamaBackend::SaveSession()
{
    if (!UserSession) return;

    auto* Save = Cast<UNexusNakamaSession>(
        UGameplayStatics::CreateSaveGameObject(UNexusNakamaSession::StaticClass()));

    Save->AuthToken    = UserSession->SessionData.AuthToken;
    Save->RefreshToken = UserSession->SessionData.RefreshToken;

    UGameplayStatics::SaveGameToSlot(Save, NEXUS_NAKAMA_SESSION_SLOT, 0);
    NEXUS_LOG("[Nakama] Session saved to slot");
}