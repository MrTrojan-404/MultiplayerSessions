// Copyright NexusMultiplayer. All Rights Reserved.

#include "UI/NexusMenuWidget.h"
#include "Core/NexusLog.h"
#include "Core/NexusMultiplayerSubsystem.h"
#include "UI/NexusServerRowWidget.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Components/CheckBox.h"
#include "Kismet/GameplayStatics.h"

// ── Init ──────────────────────────────────────────────────────────────────────

void UNexusMenuWidget::NativeConstruct()
{
    Super::NativeConstruct();

    NEXUS_LOG("[MenuWidget] NativeConstruct");

    // ── Get subsystem ──────────────────────────────────────────────────────

    UGameInstance* GI = GetGameInstance();
    if (!GI)
    {
        NEXUS_ERROR("[MenuWidget] GameInstance is null — subsystem unavailable");
        return;
    }

    UNexusMultiplayerSubsystem* Sub =
        GI->GetSubsystem<UNexusMultiplayerSubsystem>();
    if (!Sub)
    {
        NEXUS_ERROR("[MenuWidget] NexusMultiplayerSubsystem not found on GameInstance");
        return;
    }

    Subsystem = Sub;

    // ── Bind buttons ──────────────────────────────────────────────────────

    if (HostButton)
        HostButton->OnClicked.AddDynamic(this, &UNexusMenuWidget::HandleHostClicked);

    if (FindButton)
        FindButton->OnClicked.AddDynamic(this, &UNexusMenuWidget::HandleFindClicked);

    if (JoinByCodeButton)
        JoinByCodeButton->OnClicked.AddDynamic(this, &UNexusMenuWidget::HandleJoinByCodeClicked);

    if (RefreshButton)
        RefreshButton->OnClicked.AddDynamic(this, &UNexusMenuWidget::HandleRefreshClicked);

    // ── Bind subsystem delegates ──────────────────────────────────────────

    Sub->OnSessionCreated.AddDynamic(this,  &UNexusMenuWidget::HandleSessionCreated);
    Sub->OnSessionJoined.AddDynamic(this,   &UNexusMenuWidget::HandleSessionJoined);
    Sub->OnSessionFailed.AddDynamic(this,   &UNexusMenuWidget::HandleSessionFailed);
    Sub->OnSessionListReady.AddDynamic(this,&UNexusMenuWidget::HandleSessionListReady);
    Sub->OnMatchCodeReady.AddDynamic(this,  &UNexusMenuWidget::HandleMatchCodeReady);
    Sub->OnDisplayNameReady.AddDynamic(this,&UNexusMenuWidget::HandleDisplayNameReady);

    // ── Initial state ─────────────────────────────────────────────────────

    SetStatus(TEXT("Ready"));
    ShowPanel(0);

    // Display name may already be resolved if subsystem init finished before widget
    const FString Name = Sub->GetLocalDisplayName();
    if (!Name.IsEmpty() && DisplayNameText)
    {
        DisplayNameText->SetText(FText::FromString(Name));
        NEXUS_LOG("[MenuWidget] Display name already resolved: '%s'", *Name);
    }

    NEXUS_SUCCESS("[MenuWidget] Init complete");
}

void UNexusMenuWidget::NativeDestruct()
{
    // Unbind to avoid callbacks firing on a destroyed widget
    if (UNexusMultiplayerSubsystem* Sub = GetSubsystem())
    {
        Sub->OnSessionCreated.RemoveDynamic(this,  &UNexusMenuWidget::HandleSessionCreated);
        Sub->OnSessionJoined.RemoveDynamic(this,   &UNexusMenuWidget::HandleSessionJoined);
        Sub->OnSessionFailed.RemoveDynamic(this,   &UNexusMenuWidget::HandleSessionFailed);
        Sub->OnSessionListReady.RemoveDynamic(this,&UNexusMenuWidget::HandleSessionListReady);
        Sub->OnMatchCodeReady.RemoveDynamic(this,  &UNexusMenuWidget::HandleMatchCodeReady);
        Sub->OnDisplayNameReady.RemoveDynamic(this,&UNexusMenuWidget::HandleDisplayNameReady);
    }

    NEXUS_LOG("[MenuWidget] NativeDestruct — delegates unbound");
    Super::NativeDestruct();
}

// ── Button Handlers ───────────────────────────────────────────────────────────

void UNexusMenuWidget::HandleHostClicked()
{
    UNexusMultiplayerSubsystem* Sub = GetSubsystem();
    if (!Sub) return;

    FNexusSessionConfig Config = BuildSessionConfig();

    NEXUS_LOG("[MenuWidget] Host clicked. Players=%d | Map='%s' | Private=%s | Type='%s'",
        Config.MaxPlayers,
        *Config.LobbyMapPath,
        Config.bPrivate ? TEXT("YES") : TEXT("NO"),
        *Config.MatchType);

    SetStatus(TEXT("Creating session..."), FLinearColor::Yellow);
    SetLoadingState(true);

    Sub->HostSession(Config);
}

void UNexusMenuWidget::HandleFindClicked()
{
    UNexusMultiplayerSubsystem* Sub = GetSubsystem();
    if (!Sub) return;

    NEXUS_LOG("[MenuWidget] Find clicked — searching for sessions");

    SetStatus(TEXT("Searching..."), FLinearColor::Yellow);
    SetLoadingState(true);
    ClearServerList();
    ShowPanel(1);

    Sub->FindSessions(100);
}

void UNexusMenuWidget::HandleJoinByCodeClicked()
{
    UNexusMultiplayerSubsystem* Sub = GetSubsystem();
    if (!Sub) return;

    if (!CodeInputBox)
    {
        NEXUS_ERROR("[MenuWidget] JoinByCode: CodeInputBox is null — check UMG bindings");
        return;
    }

    const FString Code = CodeInputBox->GetText().ToString().TrimStartAndEnd().ToUpper();

    if (Code.IsEmpty())
    {
        NEXUS_WARN("[MenuWidget] JoinByCode: code input is empty");
        SetStatus(TEXT("Enter a match code first"), FLinearColor::Yellow);
        return;
    }

    if (Code.Len() != 6)
    {
        NEXUS_WARN("[MenuWidget] JoinByCode: code '%s' is not 6 characters", *Code);
        SetStatus(TEXT("Match code must be 6 characters"), FLinearColor::Yellow);
        return;
    }

    NEXUS_LOG("[MenuWidget] JoinByCode clicked. Code='%s'", *Code);

    SetStatus(FString::Printf(TEXT("Looking up code '%s'..."), *Code),
        FLinearColor::Yellow);
    SetLoadingState(true);

    Sub->JoinByCode(Code);
}

void UNexusMenuWidget::HandleRefreshClicked()
{
    NEXUS_LOG("[MenuWidget] Refresh clicked");
    HandleFindClicked();
}

// ── Subsystem Delegate Handlers ───────────────────────────────────────────────

void UNexusMenuWidget::HandleSessionCreated(bool bWasSuccessful)
{
    SetLoadingState(false);

    if (!bWasSuccessful)
    {
        NEXUS_ERROR("[MenuWidget] Session creation failed");
        SetStatus(TEXT("Failed to create session"), FLinearColor::Red);
        return;
    }

    NEXUS_SUCCESS("[MenuWidget] Session created — traveling to lobby");
    SetStatus(TEXT("Session created — loading lobby..."), FLinearColor::Green);
    ShowPanel(2); // Hosting panel
}

void UNexusMenuWidget::HandleSessionJoined(bool bWasSuccessful)
{
    SetLoadingState(false);

    if (!bWasSuccessful)
    {
        NEXUS_ERROR("[MenuWidget] Join failed");
        SetStatus(TEXT("Failed to join session"), FLinearColor::Red);
        return;
    }

    NEXUS_SUCCESS("[MenuWidget] Joined session — traveling to lobby");
    SetStatus(TEXT("Joined — loading lobby..."), FLinearColor::Green);
}

void UNexusMenuWidget::HandleSessionFailed(const FString& Error)
{
    SetLoadingState(false);

    NEXUS_ERROR("[MenuWidget] Session error: %s", *Error);
    SetStatus(Error, FLinearColor::Red);
}

void UNexusMenuWidget::HandleSessionListReady(const TArray<FNexusServerRow>& Rows)
{
    SetLoadingState(false);

    if (Rows.Num() == 0)
    {
        NEXUS_WARN("[MenuWidget] No sessions found");
        SetStatus(TEXT("No sessions found — try refreshing"), FLinearColor::Yellow);
        return;
    }

    NEXUS_SUCCESS("[MenuWidget] Session list ready: %d sessions", Rows.Num());
    SetStatus(FString::Printf(TEXT("%d session(s) found"), Rows.Num()),
        FLinearColor::Green);

    PopulateServerList(Rows);
}

void UNexusMenuWidget::HandleMatchCodeReady(const FString& Code)
{
    NEXUS_SUCCESS("[MenuWidget] Match code ready: '%s'", *Code);

    if (MatchCodeDisplay)
        MatchCodeDisplay->SetText(FText::FromString(Code));
}

void UNexusMenuWidget::HandleDisplayNameReady(const FString& Name)
{
    NEXUS_SUCCESS("[MenuWidget] Display name ready: '%s'", *Name);

    if (DisplayNameText)
        DisplayNameText->SetText(FText::FromString(Name));
}

// ── Server Row Handler ────────────────────────────────────────────────────────

void UNexusMenuWidget::HandleServerRowClicked(const TArray<FNexusServerRow>& Rows)
{
    if (Rows.Num() == 0) return;

    UNexusMultiplayerSubsystem* Sub = GetSubsystem();
    if (!Sub) return;

    const FNexusServerRow& Row = Rows[0];

    NEXUS_LOG("[MenuWidget] Server row clicked: Host='%s' SessionId='%s'",
        *Row.HostName, *Row.SessionId);

    SetStatus(FString::Printf(TEXT("Joining %s's session..."), *Row.HostName),
        FLinearColor::Yellow);
    SetLoadingState(true);

    Sub->JoinSessionFromRow(Row);
}

// ── Helpers ───────────────────────────────────────────────────────────────────

UNexusMultiplayerSubsystem* UNexusMenuWidget::GetSubsystem() const
{
    if (Subsystem.IsValid()) return Subsystem.Get();
    NEXUS_ERROR("[MenuWidget] GetSubsystem: subsystem ref is stale");
    return nullptr;
}

void UNexusMenuWidget::SetStatus(const FString& Message, FLinearColor Color)
{
    if (StatusText)
    {
        StatusText->SetText(FText::FromString(Message));
        StatusText->SetColorAndOpacity(FSlateColor(Color));
    }
}

void UNexusMenuWidget::SetLoadingState(bool bLoading)
{
    // Disable interactive elements while an async op is in-flight
    const bool bEnabled = !bLoading;

    if (HostButton)       HostButton->SetIsEnabled(bEnabled);
    if (FindButton)       FindButton->SetIsEnabled(bEnabled);
    if (JoinByCodeButton) JoinByCodeButton->SetIsEnabled(bEnabled);
    if (RefreshButton)    RefreshButton->SetIsEnabled(bEnabled);

    if (bLoading)
        ShowPanel(3); // Loading panel
}

void UNexusMenuWidget::ClearServerList()
{
    if (ServerListScrollBox)
        ServerListScrollBox->ClearChildren();
}

void UNexusMenuWidget::PopulateServerList(const TArray<FNexusServerRow>& Rows)
{
    ClearServerList();

    if (!ServerListScrollBox)
    {
        NEXUS_ERROR("[MenuWidget] PopulateServerList: ServerListScrollBox is null");
        return;
    }

    if (!ServerRowWidgetClass)
    {
        NEXUS_ERROR("[MenuWidget] PopulateServerList: ServerRowWidgetClass not set — "
                    "assign your row widget Blueprint in the menu widget defaults");
        return;
    }

    for (const FNexusServerRow& Row : Rows)
    {
        UNexusServerRowWidget* RowWidget =
            CreateWidget<UNexusServerRowWidget>(this, ServerRowWidgetClass);

        if (!RowWidget)
        {
            NEXUS_ERROR("[MenuWidget] Failed to create row widget for session '%s'",
                *Row.SessionId);
            continue;
        }

        RowWidget->InitRow(Row);
        RowWidget->OnServerRowClicked.AddDynamic(
            this, &UNexusMenuWidget::HandleServerRowClicked);

        ServerListScrollBox->AddChild(RowWidget);
    }

    NEXUS_LOG("[MenuWidget] Populated server list with %d rows", Rows.Num());
}

FNexusSessionConfig UNexusMenuWidget::BuildSessionConfig() const
{
    FNexusSessionConfig Config = DefaultSessionConfig;

    // Override with input box values if they're filled in
    if (MaxPlayersBox)
    {
        const FString Val = MaxPlayersBox->GetText().ToString().TrimStartAndEnd();
        if (!Val.IsEmpty())
        {
            const int32 Parsed = FCString::Atoi(*Val);
            if (Parsed > 0)
            {
                Config.MaxPlayers = FMath::Clamp(Parsed, 2, 64);
                NEXUS_LOG("[MenuWidget] MaxPlayers from input: %d", Config.MaxPlayers);
            }
        }
    }

    if (MatchTypeBox)
    {
        const FString Val = MatchTypeBox->GetText().ToString().TrimStartAndEnd();
        if (!Val.IsEmpty())
            Config.MatchType = Val;
    }

    if (LobbyMapBox)
    {
        const FString Val = LobbyMapBox->GetText().ToString().TrimStartAndEnd();
        if (!Val.IsEmpty())
            Config.LobbyMapPath = Val;
    }

    if (PrivateCheckBox)
        Config.bPrivate = PrivateCheckBox->IsChecked();

    return Config;
}

void UNexusMenuWidget::ShowPanel(int32 Index)
{
    if (MenuSwitcher)
        MenuSwitcher->SetActiveWidgetIndex(Index);
}