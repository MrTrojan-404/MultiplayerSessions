// Copyright NexusMultiplayer. All Rights Reserved.

#include "Core/NexusMultiplayerSettings.h"
#include "Core/NexusLog.h"

#if WITH_EDITOR
#include "Misc/MessageDialog.h"
#endif

UNexusMultiplayerSettings::UNexusMultiplayerSettings()
{
    CategoryName = TEXT("Plugins");
    SectionName  = TEXT("NexusMultiplayer");
}

#if WITH_EDITOR

FText UNexusMultiplayerSettings::GetSectionText() const
{
    return FText::FromString(TEXT("Nexus Multiplayer"));
}

FText UNexusMultiplayerSettings::GetSectionDescription() const
{
    return FText::FromString(
        TEXT("Configure the NexusMultiplayer plugin.\n\n"
             "Online Mode   — which platform handles sessions (Steam / EOS / Null).\n"
             "Backend Mode  — PlatformOnly needs no server. Nakama adds leaderboards, "
             "persistent storage and cross-platform identity.\n\n"
             "Changes take effect on next Play-In-Editor or packaged build launch."));
}

void UNexusMultiplayerSettings::PostEditChangeProperty(
    FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    const FName PropertyName = PropertyChangedEvent.GetPropertyName();

    // ── Nakama host validation ─────────────────────────────────────────────

    if (PropertyName == GET_MEMBER_NAME_CHECKED(UNexusMultiplayerSettings, NakamaHost))
    {
        // Strip accidental trailing slashes or whitespace
        NakamaHost = NakamaHost.TrimStartAndEnd();
        if (NakamaHost.EndsWith(TEXT("/")))
            NakamaHost = NakamaHost.LeftChop(1);

        if (BackendMode == ENexusBackendMode::Nakama && NakamaHost.IsEmpty())
        {
            ShowConfigWarning(
                TEXT("Nakama Host is empty"),
                TEXT("Nakama backend is enabled but Server Host is empty.\n"
                     "The plugin will fail to connect at runtime.\n\n"
                     "Use '127.0.0.1' for local Docker."));
        }
    }

    // ── Nakama port validation ─────────────────────────────────────────────

    if (PropertyName == GET_MEMBER_NAME_CHECKED(UNexusMultiplayerSettings, NakamaPort))
    {
        if (NakamaPort < 1 || NakamaPort > 65535)
        {
            NakamaPort = 7350;
            ShowConfigWarning(
                TEXT("Invalid Nakama Port"),
                TEXT("Port must be between 1 and 65535.\nReset to default: 7350."));
        }
    }

    // ── BackendMode → Nakama without host ─────────────────────────────────

    if (PropertyName == GET_MEMBER_NAME_CHECKED(UNexusMultiplayerSettings, BackendMode))
    {
        if (BackendMode == ENexusBackendMode::Nakama && NakamaHost.IsEmpty())
        {
            ShowConfigWarning(
                TEXT("Nakama Host Not Set"),
                TEXT("Backend set to Nakama but Server Host is empty.\n"
                     "Set the host before packaging or the plugin will fail to connect."));
        }
    }

    // ── SSL reminder for non-localhost hosts ──────────────────────────────

    if (PropertyName == GET_MEMBER_NAME_CHECKED(UNexusMultiplayerSettings, bNakamaUseSSL))
    {
        if (!bNakamaUseSSL
            && !NakamaHost.IsEmpty()
            && NakamaHost != TEXT("127.0.0.1")
            && NakamaHost != TEXT("localhost"))
        {
            ShowConfigWarning(
                TEXT("SSL Disabled on Non-Local Host"),
                TEXT("SSL is disabled but your Nakama host is not localhost.\n"
                     "Enable 'Use SSL' for production servers to encrypt auth tokens."));
        }
    }

    // ── EOS fields check ──────────────────────────────────────────────────

    if (PropertyName == GET_MEMBER_NAME_CHECKED(UNexusMultiplayerSettings, OnlineMode))
    {
        if (OnlineMode == ENexusOnlineMode::EOS
            && (EOSProductId.IsEmpty() || EOSClientId.IsEmpty()))
        {
            ShowConfigWarning(
                TEXT("EOS Credentials Not Set"),
                TEXT("Online Mode set to EOS but Product ID or Client ID is empty.\n"
                     "Fill in all EOS fields or sessions will fail to initialize."));
        }
    }

    SaveConfig();
}

void UNexusMultiplayerSettings::ShowConfigWarning(
    const FString& Title, const FString& Message)
{
    FMessageDialog::Open(
        EAppMsgType::Ok,
        FText::FromString(Message),
        FText::FromString(FString::Printf(TEXT("NexusMultiplayer — %s"), *Title)));
}

#endif // WITH_EDITOR

bool UNexusMultiplayerSettings::IsValid(FString& OutError) const
{
    // ── Online mode checks ────────────────────────────────────────────────

    if (OnlineMode == ENexusOnlineMode::EOS)
    {
        if (EOSProductId.IsEmpty())
        {
            OutError = TEXT("EOS Product ID is empty");
            return false;
        }
        if (EOSClientId.IsEmpty())
        {
            OutError = TEXT("EOS Client ID is empty");
            return false;
        }
        if (EOSClientSecret.IsEmpty())
        {
            OutError = TEXT("EOS Client Secret is empty");
            return false;
        }
        if (EOSSandboxId.IsEmpty())
        {
            OutError = TEXT("EOS Sandbox ID is empty");
            return false;
        }
        if (EOSDeploymentId.IsEmpty())
        {
            OutError = TEXT("EOS Deployment ID is empty");
            return false;
        }
    }

    // ── Backend checks ─────────────────────────────────────────────────

    if (BackendMode == ENexusBackendMode::Nakama)
    {
        if (NakamaHost.IsEmpty())
        {
            OutError = TEXT("Nakama Host is empty");
            return false;
        }
        if (NakamaPort < 1 || NakamaPort > 65535)
        {
            OutError = FString::Printf(
                TEXT("Nakama Port %d is out of range (1-65535)"), NakamaPort);
            return false;
        }
        if (NakamaServerKey.IsEmpty())
        {
            OutError = TEXT("Nakama Server Key is empty");
            return false;
        }
    }

    return true;
}