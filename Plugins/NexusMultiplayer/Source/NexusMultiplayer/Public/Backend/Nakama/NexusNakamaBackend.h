// Copyright NexusMultiplayer. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Backend/Nakama/NexusNakamaCallbackProxy.h"
#include "Interfaces/INexusBackendInterface.h"

// Nakama SDK forward declares
class UNakamaClient;
class UNakamaSession;
struct FNakamaError;
struct FNakamaAccount;

#define NEXUS_NAKAMA_SESSION_SLOT TEXT("NexusNakamaSession")
#define NEXUS_DEVICE_ID_SLOT      TEXT("NexusDeviceId")

UENUM()
enum class ENexusNakamaAuthProvider : uint8
{
    None,
    EOSConnect,  //priority 1
    Steam,
    Device,
    Email,
};

/**
 * FNexusNakamaBackend
 *
 * Backend implementation for Nakama.
 * Migrated and refactored from NakamaBackendSubsystem.
 *
 * Auth flow:
 *   Steam available  → AuthenticateSteam (requires App ID + publisher key)
 *   Steam fails      → AuthenticateDevice (fallback, always works)
 *   Restored session → skip auth, call FetchAccount directly
 *
 * Display name flow:
 *   After auth → FetchAccount → OnAccountLoaded → subsystem broadcasts name
 *   If name is device UUID → TrySetBestName → Steam nickname or OS username
 */
class NEXUSMULTIPLAYER_API FNexusNakamaBackend : public INexusBackendInterface
{
public:
    explicit FNexusNakamaBackend(const FString& Host, int32 Port,
    const FString& ServerKey, bool bUseSSL, UGameInstance* InGameInstance);
    ~FNexusNakamaBackend();

    // INexusBackendInterface
    virtual void    Login() override;
    virtual bool    IsLoggedIn() const override;
    virtual ENexusBackendMode GetMode() const override { return ENexusBackendMode::Nakama; }
    virtual FString GetDisplayName() const override;
    virtual FString GetBackendUserId() const override;
    virtual void    SetDisplayName(const FString& NewName) override;
    virtual void    FetchAccount() override;
    virtual void    SubmitScore(const FString& LeaderboardId, int64 Score) override;
    virtual void    FetchTopScores(const FString& LeaderboardId, int32 Limit) override;

    // Nakama-specific — used by FNexusNakamaMatchCode
    UNakamaClient*  GetClient()  const { return NakamaClient; }
    UNakamaSession* GetSession() const { return UserSession; }

    // True if Nakama username looks like a real name (not a device UUID)
    bool HasMeaningfulName() const;

    ENexusNakamaAuthProvider GetActiveProvider() const { return ActiveProvider; }

    // Auth callbacks
    void HandleAuthSuccess(UNakamaSession* Session);
    void HandleAuthError(const FNakamaError& Error);

    // Account callbacks
    void HandleGetAccountSuccess(const FNakamaAccount& Account);
    void HandleGetAccountError(const FNakamaError& Error);

    // Display name callbacks
    void HandleSetDisplayNameSuccess();
    void HandleSetDisplayNameError(const FNakamaError& Error);

    void HandleWriteLeaderboardSuccess(const FNakamaLeaderboardRecord& Record);
    void HandleWriteLeaderboardError(const FNakamaError& Error);
    void HandleListLeaderboardSuccess(const FNakamaLeaderboardRecordList& RecordList);
    void HandleListLeaderboardError(const FNakamaError& Error);
private:
    // ── Config ────────────────────────────────────────────────────────────────
    FString         Host;
    int32           Port;
    FString         ServerKey;
    bool            bUseSSL;
    TWeakObjectPtr<UGameInstance> GameInstance;

    UWorld* GetWorld() const { return GameInstance.IsValid() ? GameInstance->GetWorld() : nullptr; }

    // ── Nakama SDK objects ────────────────────────────────────────────────────
    UNakamaClient*  NakamaClient  = nullptr;
    UNakamaSession* UserSession   = nullptr;

    // ── State ─────────────────────────────────────────────────────────────────
    ENexusNakamaAuthProvider ActiveProvider = ENexusNakamaAuthProvider::None;
    FString PendingDisplayName;

    // ── Auth ──────────────────────────────────────────────────────────────────
    void LoginWithEOSConnect();
    void LoginWithSteam();
    void LoginWithDevice();

    // ── Session persistence ───────────────────────────────────────────────────
    bool TryRestoreSession();
    void SaveSession();

    // ── Helpers ───────────────────────────────────────────────────────────────
    FString GetEOSConnectId() const;
    FString GetSteamAuthTicket() const;
    FString GetDeviceId() const;
    void    TrySetBestAvailableName();

    TObjectPtr<UNexusNakamaBackendProxy> Proxy;

    FString PendingLeaderboardId; // tracks which board is in-flight

    bool EnsureClientAndSession(const FString& CallerName) const;

    
};