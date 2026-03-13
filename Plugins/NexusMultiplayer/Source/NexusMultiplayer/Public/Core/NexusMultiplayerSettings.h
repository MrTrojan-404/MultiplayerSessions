// Copyright NexusMultiplayer. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Core/NexusTypes.h"
#include "NexusMultiplayerSettings.generated.h"

UCLASS(Config = Game, DefaultConfig,
    meta = (DisplayName = "Nexus Multiplayer"))
class NEXUSMULTIPLAYER_API UNexusMultiplayerSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UNexusMultiplayerSettings();

    // ── Platform ──────────────────────────────────────────────────────────────

    UPROPERTY(Config, EditAnywhere, Category = "Platform",
        meta = (DisplayName = "Online Mode",
            ToolTip = "Which platform handles sessions and matchmaking.\nSteam: requires OnlineSubsystemSteam.\nEOS: requires EOS SDK.\nNull: LAN / dev / PIE — no platform required."))
    ENexusOnlineMode OnlineMode = ENexusOnlineMode::EOS;

    // ── Backend ───────────────────────────────────────────────────────────────

    UPROPERTY(Config, EditAnywhere, Category = "Backend",
        meta = (DisplayName = "Backend Mode",
            ToolTip = "PlatformOnly: no external server — Steam/EOS handle everything.\nNakama: adds persistent storage, leaderboards, and cross-platform identity."))
    ENexusBackendMode BackendMode = ENexusBackendMode::PlatformOnly;

    // ── Nakama ────────────────────────────────────────────────────────────────

    UPROPERTY(Config, EditAnywhere, Category = "Nakama",
        meta = (EditCondition = "BackendMode == ENexusBackendMode::Nakama",
            DisplayName = "Server Host",
            ToolTip = "Nakama server hostname or IP. Use '127.0.0.1' for local Docker."))
    FString NakamaHost = TEXT("127.0.0.1");

    UPROPERTY(Config, EditAnywhere, Category = "Nakama",
        meta = (EditCondition = "BackendMode == ENexusBackendMode::Nakama",
            DisplayName = "Server Port",
            ClampMin = "1", ClampMax = "65535"))
    int32 NakamaPort = 7350;

    UPROPERTY(Config, EditAnywhere, Category = "Nakama",
        meta = (EditCondition = "BackendMode == ENexusBackendMode::Nakama",
            DisplayName = "Server Key",
            ToolTip = "Nakama server key. Default is 'defaultkey' for local Docker."))
    FString NakamaServerKey = TEXT("defaultkey");

    UPROPERTY(Config, EditAnywhere, Category = "Nakama",
        meta = (EditCondition = "BackendMode == ENexusBackendMode::Nakama",
            DisplayName = "Use SSL",
            ToolTip = "Enable for production HTTPS. Disable for local Docker."))
    bool bNakamaUseSSL = false;

    // ── EOS ───────────────────────────────────────────────────────────────────

    UPROPERTY(Config, EditAnywhere, Category = "EOS",
        meta = (EditCondition = "OnlineMode == ENexusOnlineMode::EOS",
            DisplayName = "Product ID"))
    FString EOSProductId;

    UPROPERTY(Config, EditAnywhere, Category = "EOS",
        meta = (EditCondition = "OnlineMode == ENexusOnlineMode::EOS",
            DisplayName = "Client ID"))
    FString EOSClientId;

    UPROPERTY(Config, EditAnywhere, Category = "EOS",
        meta = (EditCondition = "OnlineMode == ENexusOnlineMode::EOS",
            DisplayName = "Client Secret"))
    FString EOSClientSecret;

    UPROPERTY(Config, EditAnywhere, Category = "EOS",
    meta = (EditCondition = "OnlineMode == ENexusOnlineMode::EOS",
        DisplayName = "Sandbox ID"))
    FString EOSSandboxId;

    UPROPERTY(Config, EditAnywhere, Category = "EOS",
        meta = (EditCondition = "OnlineMode == ENexusOnlineMode::EOS",
            DisplayName = "Deployment ID"))
    FString EOSDeploymentId;

    // ── Helpers ───────────────────────────────────────────────────────────────

    static const UNexusMultiplayerSettings* Get()
    {
        return GetDefault<UNexusMultiplayerSettings>();
    }

    // Returns false and fills OutError if config is invalid for the current mode
    bool IsValid(FString& OutError) const;

#if WITH_EDITOR
    virtual FText GetSectionText() const override;
    virtual FText GetSectionDescription() const override;
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

private:
    static void ShowConfigWarning(const FString& Title, const FString& Message);
#endif
};