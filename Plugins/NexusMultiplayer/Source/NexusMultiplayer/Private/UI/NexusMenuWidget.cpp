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
#include "Components/ComboBoxString.h"

// ── Init ──────────────────────────────────────────────────────────────────────

void UNexusMenuWidget::NativeConstruct()
{
    Super::NativeConstruct();

    NEXUS_LOG("[MenuWidget] NativeConstruct");

    // ── Input mode ────────────────────────────────────────────────────────

    if (APlayerController* PC = GetOwningPlayer())
    {
        FInputModeUIOnly InputMode;
        InputMode.SetWidgetToFocus(TakeWidget());
        InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        PC->SetInputMode(InputMode);
        PC->SetShowMouseCursor(true);
    }

    // ── Subsystem ─────────────────────────────────────────────────────────

    UGameInstance* GI = GetGameInstance();
    if (!GI)
    {
        NEXUS_ERROR("[MenuWidget] GameInstance is null");
        return;
    }

    UNexusMultiplayerSubsystem* Sub =
        GI->GetSubsystem<UNexusMultiplayerSubsystem>();
    if (!Sub)
    {
        NEXUS_ERROR("[MenuWidget] NexusMultiplayerSubsystem not found");
        return;
    }

    Subsystem = Sub;

    if (!Sub->IsJoinByCodeSupported())
    {
        if (JoinByCodeButton) JoinByCodeButton->SetVisibility(ESlateVisibility::Collapsed);
        if (CodeInputBox)     CodeInputBox->SetVisibility(ESlateVisibility::Collapsed);
        NEXUS_LOG("[MenuWidget] JoinByCode not supported in current config — hiding UI");
    }

    // ── Bind buttons ──────────────────────────────────────────────────────

    if (HostButton)
        HostButton->OnClicked.AddDynamic(
            this, &UNexusMenuWidget::HandleHostClicked);

    if (FindButton)
        FindButton->OnClicked.AddDynamic(
            this, &UNexusMenuWidget::HandleFindClicked);

    if (JoinByCodeButton)
        JoinByCodeButton->OnClicked.AddDynamic(
            this, &UNexusMenuWidget::HandleJoinByCodeClicked);

    if (RefreshButton)
        RefreshButton->OnClicked.AddDynamic(
            this, &UNexusMenuWidget::HandleRefreshClicked);

    if (ConfirmHostButton)
        ConfirmHostButton->OnClicked.AddDynamic(
            this, &UNexusMenuWidget::HandleConfirmHostClicked);
    
    if (BrowserBackButton)
        BrowserBackButton->OnClicked.AddDynamic(
            this, &UNexusMenuWidget::HandleBackClicked);

    if (HostingBackButton)
        HostingBackButton->OnClicked.AddDynamic(
            this, &UNexusMenuWidget::HandleBackClicked);

    if (LANCheckBox)
    {
        // Seed from subsystem's current mode
        LANCheckBox->SetIsChecked(
            Sub->GetNetworkMode() == ENexusNetworkMode::LAN);

        LANCheckBox->OnCheckStateChanged.AddDynamic(
            this, &UNexusMenuWidget::HandleLANCheckboxChanged);
    }

    // ── Bind subsystem delegates ──────────────────────────────────────────

    Sub->OnSessionCreated.AddDynamic(
        this, &UNexusMenuWidget::HandleSessionCreated);
    Sub->OnSessionJoined.AddDynamic(
        this, &UNexusMenuWidget::HandleSessionJoined);
    Sub->OnSessionFailed.AddDynamic(
        this, &UNexusMenuWidget::HandleSessionFailed);
    Sub->OnSessionListReady.AddDynamic(
        this, &UNexusMenuWidget::HandleSessionListReady);
    Sub->OnDisplayNameReady.AddDynamic(
        this, &UNexusMenuWidget::HandleDisplayNameReady);

    // Populate match type options
    if (MatchTypeCombo)
    {
        MatchTypeCombo->ClearOptions();
        MatchTypeCombo->AddOption(TEXT("FreeForAll"));
       // MatchTypeCombo->AddOption(TEXT("TeamDeathmatch"));
        MatchTypeCombo->SetSelectedOption(DefaultSessionConfig.MatchType);
    }

    // Populate map options
    if (MapCombo)
    {
        MapCombo->ClearOptions();
        for (const FNexusMapEntry& Map : AvailableMaps)
            MapCombo->AddOption(Map.DisplayName);

        if (MapCombo->GetOptionCount() > 0)
            MapCombo->SetSelectedIndex(0);

        MapCombo->OnSelectionChanged.AddDynamic(
            this, &UNexusMenuWidget::HandleMapSelectionChanged);

        if (AvailableMaps.Num() > 0)
            SelectedMap = AvailableMaps[0];
    }
    
    // ── Initial state ─────────────────────────────────────────────────────

    ShowMain();

    // Display name may already be available if subsystem init beat widget init
    const FString Name = Sub->GetLocalDisplayName();
    if (!Name.IsEmpty() && DisplayNameText)
        DisplayNameText->SetText(FText::FromString(Name));

    NEXUS_SUCCESS("[MenuWidget] Init complete");
}

void UNexusMenuWidget::NativeDestruct()
{
    if (APlayerController* PC = GetOwningPlayer())
    {
        PC->SetInputMode(FInputModeGameOnly());
        PC->SetShowMouseCursor(false);
    }

    if (UNexusMultiplayerSubsystem* Sub = GetSubsystem())
    {
        Sub->OnSessionCreated.RemoveDynamic(
            this, &UNexusMenuWidget::HandleSessionCreated);
        Sub->OnSessionJoined.RemoveDynamic(
            this, &UNexusMenuWidget::HandleSessionJoined);
        Sub->OnSessionFailed.RemoveDynamic(
            this, &UNexusMenuWidget::HandleSessionFailed);
        Sub->OnSessionListReady.RemoveDynamic(
            this, &UNexusMenuWidget::HandleSessionListReady);
        Sub->OnDisplayNameReady.RemoveDynamic(
            this, &UNexusMenuWidget::HandleDisplayNameReady);
    }

    NEXUS_LOG("[MenuWidget] NativeDestruct — delegates unbound");
    Super::NativeDestruct();
}

// ── Button Handlers ───────────────────────────────────────────────────────────

void UNexusMenuWidget::HandleHostClicked()
{
    // Go to hosting config panel first — user fills in settings then confirms
    NEXUS_LOG("[MenuWidget] Host clicked — showing hosting config");
    ShowHosting();
}

void UNexusMenuWidget::HandleConfirmHostClicked()
{
    UNexusMultiplayerSubsystem* Sub = GetSubsystem();
    if (!Sub) return;

    FNexusSessionConfig Config = BuildSessionConfig();

    NEXUS_LOG("[MenuWidget] ConfirmHost: Players=%d | Map='%s' | Private=%s | Type='%s'",
        Config.MaxPlayers,
        *Config.LobbyMapPath,
        Config.bPrivate ? TEXT("YES") : TEXT("NO"),
        *Config.MatchType);

    ShowLoading(TEXT("Creating session..."));
    Sub->HostSession(Config);
}

void UNexusMenuWidget::HandleFindClicked()
{
    UNexusMultiplayerSubsystem* Sub = GetSubsystem();
    if (!Sub) return;

    NEXUS_LOG("[MenuWidget] Find clicked");

    ClearServerList();
    ShowLoading(TEXT("Searching for sessions..."));
    Sub->FindSessions(100);
}

void UNexusMenuWidget::HandleJoinByCodeClicked()
{
    UNexusMultiplayerSubsystem* Sub = GetSubsystem();
    if (!Sub) return;

    if (!CodeInputBox)
    {
        NEXUS_ERROR("[MenuWidget] CodeInputBox is null");
        return;
    }

    const FString Code = CodeInputBox->GetText()
        .ToString().TrimStartAndEnd().ToUpper();

    if (Code.IsEmpty())
    {
        SetStatus(TEXT("Enter a match code first"), FLinearColor::Yellow);
        return;
    }

    if (Code.Len() != 6)
    {
        SetStatus(TEXT("Match code must be 6 characters"), FLinearColor::Yellow);
        return;
    }

    NEXUS_LOG("[MenuWidget] JoinByCode: '%s'", *Code);

    ShowLoading(FString::Printf(TEXT("Looking up code '%s'..."), *Code));
    Sub->JoinByCode(Code);
}

void UNexusMenuWidget::HandleRefreshClicked()
{
    NEXUS_LOG("[MenuWidget] Refresh clicked");
    HandleFindClicked();
}

void UNexusMenuWidget::HandleBackClicked()
{
    NEXUS_LOG("[MenuWidget] Back clicked — returning to Main");
    ShowMain();
}

// ── Subsystem Delegate Handlers ───────────────────────────────────────────────

void UNexusMenuWidget::HandleSessionCreated(bool bWasSuccessful)
{
    if (!bWasSuccessful)
    {
        NEXUS_ERROR("[MenuWidget] Session creation failed");
        ShowMain();
        SetStatus(TEXT("Failed to create session. Try again."), FLinearColor::Red);
        return;
    }

    // Stay on loading — ServerTravel is already in progress,
    // widget will be destroyed when the lobby map loads
    NEXUS_SUCCESS("[MenuWidget] Session created — traveling to lobby");
    SetStatus(TEXT("Loading lobby..."), FLinearColor::Green);
}

void UNexusMenuWidget::HandleSessionJoined(bool bWasSuccessful)
{
    if (!bWasSuccessful)
    {
        NEXUS_ERROR("[MenuWidget] Join failed");
        ShowMain();
        SetStatus(TEXT("Failed to join session. Try again."), FLinearColor::Red);
        return;
    }

    // Success — subsystem handles ClientTravel, widget will be destroyed shortly
    NEXUS_SUCCESS("[MenuWidget] Joined session — traveling");
    SetStatus(TEXT("Joined — loading..."), FLinearColor::Green);
}

void UNexusMenuWidget::HandleSessionFailed(const FString& Error)
{
    NEXUS_ERROR("[MenuWidget] Session error: %s", *Error);
    ShowMain();
    SetStatus(FString::Printf(TEXT("Error: %s"), *Error), FLinearColor::Red);
}

void UNexusMenuWidget::HandleSessionListReady(const TArray<FNexusServerRow>& Rows)
{
    if (Rows.Num() == 0)
    {
        NEXUS_WARN("[MenuWidget] No sessions found");
        ShowMain();
        SetStatus(TEXT("No sessions found — try refreshing"), FLinearColor::Yellow);
        return;
    }

    NEXUS_SUCCESS("[MenuWidget] %d session(s) found", Rows.Num());
    PopulateServerList(Rows);
    ShowBrowser();
    SetStatus(FString::Printf(TEXT("%d session(s) found"), Rows.Num()),
        FLinearColor::Green);
}

void UNexusMenuWidget::HandleDisplayNameReady(const FString& Name)
{
    NEXUS_SUCCESS("[MenuWidget] Display name: '%s'", *Name);
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

    NEXUS_LOG("[MenuWidget] Row clicked: Host='%s'", *Row.HostName);

    ShowLoading(FString::Printf(
        TEXT("Joining %s's session..."), *Row.HostName));

    Sub->JoinSessionFromRow(Row);
}

// ── Panel Helpers ─────────────────────────────────────────────────────────────

void UNexusMenuWidget::ShowMain()
{
    if (MenuSwitcher) MenuSwitcher->SetActiveWidgetIndex(0);
    SetButtonsEnabled(true);
    SetStatus(TEXT("Ready"));
}

void UNexusMenuWidget::ShowBrowser()
{
    if (MenuSwitcher) MenuSwitcher->SetActiveWidgetIndex(1);
    SetButtonsEnabled(true);
}

void UNexusMenuWidget::ShowHosting()
{
    if (MenuSwitcher) MenuSwitcher->SetActiveWidgetIndex(2);
    SetButtonsEnabled(true);
}

void UNexusMenuWidget::ShowLoading(const FString& Message)
{
    if (MenuSwitcher) MenuSwitcher->SetActiveWidgetIndex(3);
    SetButtonsEnabled(false);
    SetStatus(Message, FLinearColor::Yellow);
}

// ── Other Helpers ─────────────────────────────────────────────────────────────

UNexusMultiplayerSubsystem* UNexusMenuWidget::GetSubsystem() const
{
    if (Subsystem.IsValid()) return Subsystem.Get();
    NEXUS_ERROR("[MenuWidget] Subsystem ref is stale");
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

void UNexusMenuWidget::SetButtonsEnabled(bool bEnabled)
{
    if (HostButton)         HostButton->SetIsEnabled(bEnabled);
    if (FindButton)         FindButton->SetIsEnabled(bEnabled);
    if (JoinByCodeButton)   JoinByCodeButton->SetIsEnabled(bEnabled);
    if (RefreshButton)      RefreshButton->SetIsEnabled(bEnabled);
    if (ConfirmHostButton)  ConfirmHostButton->SetIsEnabled(bEnabled);
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
        NEXUS_ERROR("[MenuWidget] ServerListScrollBox is null");
        return;
    }
    if (!ServerRowWidgetClass)
    {
        NEXUS_ERROR("[MenuWidget] ServerRowWidgetClass not set in Blueprint defaults");
        return;
    }

    for (const FNexusServerRow& Row : Rows)
    {
        UNexusServerRowWidget* RowWidget =
            CreateWidget<UNexusServerRowWidget>(this, ServerRowWidgetClass);

        if (!RowWidget)
        {
            NEXUS_ERROR("[MenuWidget] Failed to create row widget");
            continue;
        }

        RowWidget->InitRow(Row);
        RowWidget->OnServerRowClicked.AddDynamic(
            this, &UNexusMenuWidget::HandleServerRowClicked);

        ServerListScrollBox->AddChild(RowWidget);
    }

    NEXUS_LOG("[MenuWidget] Populated %d server rows", Rows.Num());
}
FNexusSessionConfig UNexusMenuWidget::BuildSessionConfig() const
{
    FNexusSessionConfig Config = DefaultSessionConfig;

    if (MaxPlayersBox)
    {
        const FString Val = MaxPlayersBox->GetText().ToString().TrimStartAndEnd();
        if (!Val.IsEmpty())
        {
            const int32 Parsed = FCString::Atoi(*Val);
            if (Parsed > 0)
                Config.MaxPlayers = FMath::Clamp(Parsed, 2, 64);
        }
    }

    if (MatchTypeCombo && MatchTypeCombo->GetOptionCount() > 0)
        Config.MatchType = MatchTypeCombo->GetSelectedOption();

    // LobbyMapPath comes from the selected map entry
    if (!SelectedMap.LobbyMapPath.IsEmpty())
    {
        Config.LobbyMapPath = SelectedMap.LobbyMapPath;
        Config.GameMapPath  = SelectedMap.GameMapPath;
    }

    if (PrivateCheckBox)
        Config.bPrivate = PrivateCheckBox->IsChecked();

    return Config;
}

void UNexusMenuWidget::HandleMapSelectionChanged(
    FString SelectedItem, ESelectInfo::Type SelectionType)
{
    for (const FNexusMapEntry& Map : AvailableMaps)
    {
        if (Map.DisplayName == SelectedItem)
        {
            SelectedMap = Map;
            NEXUS_LOG("[MenuWidget] Map selected: '%s' | Lobby='%s' | Game='%s'",
                *Map.DisplayName, *Map.LobbyMapPath, *Map.GameMapPath);
            return;
        }
    }
}

void UNexusMenuWidget::HandleLANCheckboxChanged(bool bIsChecked)
{
    UNexusMultiplayerSubsystem* Sub = GetSubsystem();
    if (!Sub) return;

    Sub->SetNetworkMode(bIsChecked
        ? ENexusNetworkMode::LAN
        : ENexusNetworkMode::Internet);

    // Hide/show JoinByCode based on mode + support
    const bool bShowCode = !bIsChecked && Sub->IsJoinByCodeSupported();
    if (JoinByCodeButton) JoinByCodeButton->SetVisibility(
        bShowCode ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    if (CodeInputBox) CodeInputBox->SetVisibility(
        bShowCode ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

    NEXUS_LOG("[MenuWidget] LAN mode: %s", bIsChecked ? TEXT("ON") : TEXT("OFF"));
}