// Copyright NexusMultiplayer. All Rights Reserved.

#include "Backend/Nakama/NexusNakamaBackend.h"
#include "Core/NexusLog.h"
#include "NakamaClient.h"
#include "NakamaSession.h"
#include "OnlineSubsystemUtils.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Kismet/GameplayStatics.h"
#include "SaveGame/NexusNakamaSession.h"

// ── Constructor ───────────────────────────────────────────────────────────────

FNexusNakamaBackend::FNexusNakamaBackend(
    const FString& InHost,
    int32          InPort,
    const FString& InServerKey,
    bool           bInUseSSL,
    UWorld*        InWorld)
    : Host(InHost)
    , Port(InPort)
    , ServerKey(InServerKey)
    , bUseSSL(bInUseSSL)
    , World(InWorld)
{
    NEXUS_LOG("[Nakama] Backend created. Host: %s:%d | SSL: %s",
        *Host, Port, bUseSSL ? TEXT("YES") : TEXT("NO"));

    NakamaClient = UNakamaClient::CreateDefaultClient(
        ServerKey, Host, Port, bUseSSL, /*bEnableDebug=*/true);

    if (!NakamaClient)
    {
        NEXUS_ERROR("[Nakama] Failed to create NakamaClient — check host/port/key");
    }
}

// ── Login ─────────────────────────────────────────────────────────────────────

void FNexusNakamaBackend::Login()
{
    NEXUS_LOG("[Nakama] Login called");

    if (!NakamaClient)
    {
        NEXUS_ERROR("[Nakama] Login failed — NakamaClient is null");
        if (OnLoginError) OnLoginError(TEXT("NakamaClient not initialized"));
        return;
    }

    // Try to restore a saved session first — skips full auth roundtrip
    if (TryRestoreSession())
    {
        NEXUS_SUCCESS("[Nakama] Session restored — skipping full auth");
        return;
    }

    NEXUS_LOG("[Nakama] No valid saved session — starting fresh auth");

    // Pick auth provider based on what's available
    IOnlineSubsystem* OSS = Online::GetSubsystem(World.Get());
    const bool bSteamAvailable =
        OSS && OSS->GetSubsystemName() == FName("STEAM");

    if (bSteamAvailable)
    {
        NEXUS_LOG("[Nakama] Steam OSS detected — attempting Steam auth");
        LoginWithSteam();
    }
    else
    {
        NEXUS_WARN("[Nakama] Steam not available — using Device auth");
        LoginWithDevice();
    }
}

void FNexusNakamaBackend::LoginWithSteam()
{
    FString Ticket = GetSteamAuthTicket();

    if (Ticket.IsEmpty())
    {
        NEXUS_WARN("[Nakama] Steam ticket empty — Steam may not have App ID configured. Falling back to Device auth");
        LoginWithDevice();
        return;
    }

    NEXUS_LOG("[Nakama] Authenticating with Steam. Ticket length: %d", Ticket.Len());

    ActiveProvider = ENexusNakamaAuthProvider::Steam;

    FOnAuthUpdate Success;
    Success.AddRaw(this, &FNexusNakamaBackend::HandleAuthSuccess);

    FOnError Error;
    Error.AddRaw(this, &FNexusNakamaBackend::HandleAuthError);

    NakamaClient->AuthenticateSteam(
        Ticket,
        TEXT(""),   // username — Nakama generates from Steam display name
        true,       // create if not exists
        true,       // import Steam friends
        {},
        Success,
        Error);
}

void FNexusNakamaBackend::LoginWithDevice()
{
    FString DeviceId = GetDeviceId();

    NEXUS_LOG("[Nakama] Authenticating with Device ID: %s", *DeviceId.Left(8));

    ActiveProvider = ENexusNakamaAuthProvider::Device;

    FOnAuthUpdate Success;
    Success.AddRaw(this, &FNexusNakamaBackend::HandleAuthSuccess);

    FOnError Error;
    Error.AddRaw(this, &FNexusNakamaBackend::HandleAuthError);

    NakamaClient->AuthenticateDevice(DeviceId, TEXT(""), true, {}, Success, Error);
}

// ── Auth Callbacks ────────────────────────────────────────────────────────────

void FNexusNakamaBackend::HandleAuthSuccess(UNakamaSession* Session)
{
    UserSession = Session;

    NEXUS_SUCCESS("[Nakama] Auth success. Provider: %d | User: %s",
        (int32)ActiveProvider,
        *Session->SessionData.Username);

    SaveSession();
    FetchAccount();
}

void FNexusNakamaBackend::HandleAuthError(const FNakamaError& Error)
{
    NEXUS_ERROR("[Nakama] Auth error: %s", *Error.Message);

    // Steam auth fails without a valid App ID + publisher key.
    // Automatically fall back to device auth so features still work.
    if (ActiveProvider == ENexusNakamaAuthProvider::Steam)
    {
        NEXUS_WARN("[Nakama] Steam auth failed — falling back to Device auth");
        LoginWithDevice();
        return;
    }

    // Device auth failing means Nakama server is unreachable
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

    NEXUS_LOG("[Nakama] FetchAccount called. User: %s",
        *UserSession->SessionData.Username);

    FOnUserAccountInfo Success;
    Success.AddRaw(this, &FNexusNakamaBackend::HandleGetAccountSuccess);

    FOnError Error;
    Error.AddRaw(this, &FNexusNakamaBackend::HandleGetAccountError);

    NakamaClient->GetAccount(UserSession, Success, Error);
}

void FNexusNakamaBackend::HandleGetAccountSuccess(const FNakamaAccount& Account)
{
    if (UserSession)
        UserSession->SessionData.Username = Account.User.Username;

    NEXUS_SUCCESS("[Nakama] Account loaded. Username: %s | ID: %s",
        *Account.User.Username,
        *Account.User.Id);

    // If name looks like a device UUID or steam_ prefix, try to get a real name
    if (!HasMeaningfulName())
    {
        NEXUS_WARN("[Nakama] Username '%s' looks auto-generated — resolving better name",
            *Account.User.Username);
        TrySetBestAvailableName();
    }

    if (OnAccountLoaded) OnAccountLoaded();
}

void FNexusNakamaBackend::HandleGetAccountError(const FNakamaError& Error)
{
    NEXUS_ERROR("[Nakama] FetchAccount failed: %s", *Error.Message);
    // Still fire OnAccountLoaded so the subsystem doesn't stall
    if (OnAccountLoaded) OnAccountLoaded();
}

// ── Display Name ──────────────────────────────────────────────────────────────

void FNexusNakamaBackend::SetDisplayName(const FString& NewName)
{
    if (!UserSession || !NakamaClient)
    {
        NEXUS_ERROR("[Nakama] SetDisplayName: not logged in");
        return;
    }

    NEXUS_LOG("[Nakama] Setting display name: %s", *NewName);

    PendingDisplayName = NewName;

    FOnUpdateAccount Success;
    Success.AddRaw(this, &FNexusNakamaBackend::HandleSetDisplayNameSuccess);

    FOnError Error;
    Error.AddRaw(this, &FNexusNakamaBackend::HandleSetDisplayNameError);

    NakamaClient->UpdateAccount(
        UserSession,
        NewName,
        TEXT(""), TEXT(""), TEXT(""), TEXT(""), TEXT(""),
        Success, Error);
}

void FNexusNakamaBackend::HandleSetDisplayNameSuccess()
{
    if (UserSession)
        UserSession->SessionData.Username = PendingDisplayName;

    NEXUS_SUCCESS("[Nakama] Display name updated: %s", *PendingDisplayName);

    if (OnDisplayNameUpdated) OnDisplayNameUpdated(PendingDisplayName);
}

void FNexusNakamaBackend::HandleSetDisplayNameError(const FNakamaError& Error)
{
    NEXUS_ERROR("[Nakama] SetDisplayName failed: %s", *Error.Message);
    // Fire with current name so UI doesn't hang
    if (OnDisplayNameUpdated) OnDisplayNameUpdated(GetDisplayName());
}

// ── Leaderboards ──────────────────────────────────────────────────────────────

void FNexusNakamaBackend::SubmitScore(
    const FString& LeaderboardId, int64 Score)
{
    // TODO: implement Nakama leaderboard write
    NEXUS_WARN("[Nakama] SubmitScore not yet implemented. Board: %s | Score: %lld",
        *LeaderboardId, Score);
}

void FNexusNakamaBackend::FetchTopScores(
    const FString& LeaderboardId, int32 Limit)
{
    // TODO: implement Nakama leaderboard read
    NEXUS_WARN("[Nakama] FetchTopScores not yet implemented. Board: %s | Limit: %d",
        *LeaderboardId, Limit);
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

bool FNexusNakamaBackend::HasMeaningfulName() const
{
    if (!UserSession) return false;
    const FString& Name = UserSession->SessionData.Username;
    if (Name.IsEmpty()) return false;
    // Auto-generated names from Nakama: steam_ prefix or random UUID-style strings
    if (Name.StartsWith(TEXT("steam_"))) return false;
    // Device auth generates names like "ciUOXXMvoO" — 10 random alphanumeric chars
    // Heuristic: if all alphanumeric with mixed case and no spaces, likely a UUID
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

// ── Session Persistence ───────────────────────────────────────────────────────

bool FNexusNakamaBackend::TryRestoreSession()
{
    if (!UGameplayStatics::DoesSaveGameExist(TEXT("NexusNakamaSession"), 0))
    {
        NEXUS_LOG("[Nakama] No saved session found");
        return false;
    }

    auto* Save = Cast<UNexusNakamaSession>(
        UGameplayStatics::LoadGameFromSlot(TEXT("NexusNakamaSession"), 0));

    if (!Save || Save->AuthToken.IsEmpty())
    {
        NEXUS_WARN("[Nakama] Saved session file exists but is empty or corrupt");
        return false;
    }

    UserSession = UNakamaSession::RestoreSession(
        Save->AuthToken, Save->RefreshToken);

    if (!UserSession || UserSession->IsExpired())
    {
        NEXUS_WARN("[Nakama] Saved session expired — need fresh auth");
        UserSession = nullptr;
        return false;
    }

    NEXUS_SUCCESS("[Nakama] Session restored successfully. Fetching account...");
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

    UGameplayStatics::SaveGameToSlot(Save, TEXT("NexusNakamaSession"), 0);

    NEXUS_LOG("[Nakama] Session saved to slot");
}

// ── Helpers ───────────────────────────────────────────────────────────────────

void FNexusNakamaBackend::TrySetBestAvailableName()
{
    // Priority 1 — Steam display name (best, real identity)
    IOnlineSubsystem* OSS = Online::GetSubsystem(World.Get());
    if (OSS)
    {
        IOnlineIdentityPtr Identity = OSS->GetIdentityInterface();
        if (Identity.IsValid())
        {
            FString SteamName = Identity->GetPlayerNickname(0);
            if (!SteamName.IsEmpty())
            {
                NEXUS_LOG("[Nakama] Using Steam display name: %s", *SteamName);
                SetDisplayName(SteamName);
                return;
            }
        }
    }

    // Priority 2 — OS username
    FString OSName = FPlatformProcess::UserName(false);
    if (!OSName.IsEmpty())
    {
        NEXUS_LOG("[Nakama] Using OS username: %s", *OSName);
        SetDisplayName(OSName);
        return;
    }

    // Priority 3 — Trim the UUID to something readable
    FString Trimmed = FString::Printf(TEXT("Player_%s"),
        *GetDisplayName().Left(5).ToUpper());
    NEXUS_WARN("[Nakama] No better name available — using trimmed UUID: %s", *Trimmed);
    SetDisplayName(Trimmed);
}

FString FNexusNakamaBackend::GetSteamAuthTicket() const
{
    IOnlineSubsystem* OSS = Online::GetSubsystem(World.Get());
    if (!OSS) return FString();
    IOnlineIdentityPtr Identity = OSS->GetIdentityInterface();
    if (!Identity.IsValid()) return FString();
    return Identity->GetAuthToken(0);
}

FString FNexusNakamaBackend::GetDeviceId() const
{
    static FString CachedId;
    if (!CachedId.IsEmpty()) return CachedId;

    // Try loading previously persisted device ID
    if (UGameplayStatics::DoesSaveGameExist(TEXT("NexusDeviceId"), 0))
    {
        if (auto* Save = Cast<UNexusNakamaSession>(
            UGameplayStatics::LoadGameFromSlot(TEXT("NexusDeviceId"), 0)))
        {
            if (!Save->AuthToken.IsEmpty())
            {
                CachedId = Save->AuthToken;
                NEXUS_LOG("[Nakama] Device ID loaded from disk: %s", *CachedId.Left(8));
                return CachedId;
            }
        }
    }

    // Generate new persistent device ID
    CachedId = FGuid::NewGuid().ToString();

    auto* Save = Cast<UNexusNakamaSession>(
        UGameplayStatics::CreateSaveGameObject(UNexusNakamaSession::StaticClass()));
    Save->AuthToken = CachedId;
    UGameplayStatics::SaveGameToSlot(Save, TEXT("NexusDeviceId"), 0);

    NEXUS_LOG("[Nakama] New Device ID generated and saved: %s", *CachedId.Left(8));
    return CachedId;
}