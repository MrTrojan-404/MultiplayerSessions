// Copyright NexusMultiplayer. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Interfaces/INexusBackendInterface.h"

// Nakama SDK forward declares
class UNakamaClient;
class UNakamaSession;
struct FNakamaError;
struct FNakamaAccount;

UENUM()
enum class ENexusNakamaAuthProvider : uint8
{
    None,
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
    FNexusNakamaBackend(
        const FString& Host,
        int32          Port,
        const FString& ServerKey,
        bool           bUseSSL,
        UWorld*        InWorld);

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

private:
    // ── Config ────────────────────────────────────────────────────────────────
    FString         Host;
    int32           Port;
    FString         ServerKey;
    bool            bUseSSL;
    TWeakObjectPtr<UWorld> World;

    // ── Nakama SDK objects ────────────────────────────────────────────────────
    UNakamaClient*  NakamaClient  = nullptr;
    UNakamaSession* UserSession   = nullptr;

    // ── State ─────────────────────────────────────────────────────────────────
    ENexusNakamaAuthProvider ActiveProvider = ENexusNakamaAuthProvider::None;
    FString PendingDisplayName;

    // ── Auth ──────────────────────────────────────────────────────────────────
    void LoginWithSteam();
    void LoginWithDevice();

    // Auth callbacks
    UFUNCTION() void HandleAuthSuccess(UNakamaSession* Session);
    UFUNCTION() void HandleAuthError(const FNakamaError& Error);

    // Account callbacks
    UFUNCTION() void HandleGetAccountSuccess(const FNakamaAccount& Account);
    UFUNCTION() void HandleGetAccountError(const FNakamaError& Error);

    // Display name callbacks
    UFUNCTION() void HandleSetDisplayNameSuccess();
    UFUNCTION() void HandleSetDisplayNameError(const FNakamaError& Error);

    // ── Session persistence ───────────────────────────────────────────────────
    bool TryRestoreSession();
    void SaveSession();

    // ── Helpers ───────────────────────────────────────────────────────────────
    FString GetSteamAuthTicket() const;
    FString GetDeviceId() const;
    void    TrySetBestAvailableName();
};