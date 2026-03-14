// Copyright NexusMultiplayer. All Rights Reserved.

#include "UI/NexusLobbyWidget.h"
#include "GameState/NexusLobbyGameState.h"
#include "Core/NexusMultiplayerSubsystem.h"
#include "Core/NexusLog.h"
#include "Core/NexusTypes.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Interfaces/INexusOnlineInterface.h"


// ── Init ──────────────────────────────────────────────────────────────────────

void UNexusLobbyWidget::NativeConstruct()
{
    Super::NativeConstruct();

    NEXUS_LOG("[LobbyWidget] NativeConstruct");

    // ── Input mode ────────────────────────────────────────────────────────

    if (APlayerController* PC = GetOwningPlayer())
    {
        // Game + UI so the player can still look around in the lobby
        FInputModeGameAndUI InputMode;
        InputMode.SetWidgetToFocus(TakeWidget());
        InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        InputMode.SetHideCursorDuringCapture(false);
        PC->SetInputMode(InputMode);
        PC->SetShowMouseCursor(true);
    }

    if (MatchCodePanel)
        MatchCodePanel->SetVisibility(ESlateVisibility::Collapsed);
    if (MatchCodeText)
        MatchCodeText->SetVisibility(ESlateVisibility::Collapsed);
    
    // ── Subsystem ─────────────────────────────────────────────────────────

    if (UGameInstance* GI = GetGameInstance())
    {
        UNexusMultiplayerSubsystem* Sub =
            GI->GetSubsystem<UNexusMultiplayerSubsystem>();

        if (Sub)
        {
            Subsystem = Sub;
            Sub->OnSessionFailed.AddDynamic(
                this, &UNexusLobbyWidget::HandleSessionFailed);
        }
        else
        {
            NEXUS_ERROR("[LobbyWidget] NexusMultiplayerSubsystem not found");
        }
    }

    // ── Buttons ───────────────────────────────────────────────────────────

    if (LeaveButton)
        LeaveButton->OnClicked.AddDynamic(
            this, &UNexusLobbyWidget::HandleLeaveClicked);

    if (StartButton)
        StartButton->OnClicked.AddDynamic(
            this, &UNexusLobbyWidget::HandleStartClicked);
    
    // ── Initial state ─────────────────────────────────────────────────────

    SetStatus(TEXT("Waiting for players..."));

    // GameState may not be replicated yet — TryBindGameState retries on tick
    TryBindGameState();

    NEXUS_SUCCESS("[LobbyWidget] Init complete");
}

void UNexusLobbyWidget::NativeDestruct()
{
    if (APlayerController* PC = GetOwningPlayer())
    {
        PC->SetInputMode(FInputModeGameOnly());
        PC->SetShowMouseCursor(false);
    }

    // Unbind subsystem delegates
    if (UNexusMultiplayerSubsystem* Sub = GetSubsystem())
    {
        Sub->OnSessionFailed.RemoveDynamic(
            this, &UNexusLobbyWidget::HandleSessionFailed);
    }

    // Unbind GameState delegates
    if (ANexusLobbyGameState* GS = GetNexusGameState())
    {
        GS->OnMatchCodeReady.RemoveDynamic(
            this, &UNexusLobbyWidget::HandleMatchCodeReady);
        GS->OnPlayerListUpdated.RemoveDynamic(
            this, &UNexusLobbyWidget::HandlePlayerListUpdated);
    }

    NEXUS_LOG("[LobbyWidget] NativeDestruct — delegates unbound");
    Super::NativeDestruct();
}

void UNexusLobbyWidget::NativeTick(
    const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    // Retry binding GameState every tick until it's replicated
    if (!bGameStateBound)
        TryBindGameState();
}

// ── Setup ─────────────────────────────────────────────────────────────────────

void UNexusLobbyWidget::TryBindGameState()
{
    ANexusLobbyGameState* GS = GetNexusGameState();
    if (!GS) return;

    NEXUS_LOG("[LobbyWidget] GameState found — binding delegates");

    if (!GS->OnMatchCodeReady.IsAlreadyBound(this, &UNexusLobbyWidget::HandleMatchCodeReady))
    {
        GS->OnMatchCodeReady.AddDynamic(this, &UNexusLobbyWidget::HandleMatchCodeReady);
    }
    GS->OnPlayerListUpdated.AddDynamic(this, &UNexusLobbyWidget::HandlePlayerListUpdated);

    bGameStateBound = true;

    // Race condition fix — data may already exist before we bound
    const FString ExistingCode = GS->GetMatchCode();
    NEXUS_LOG("[LobbyWidget] TryBindGameState — existing code: '%s'", *ExistingCode);  
    if (!ExistingCode.IsEmpty())
        HandleMatchCodeReady(ExistingCode);

    const TArray<FNexusPlayerInfo>& Players = GS->GetPlayerList();
    if (Players.Num() > 0)
        HandlePlayerListUpdated(Players);

    SetupForRole();
}

void UNexusLobbyWidget::SetupForRole()
{
    // Use subsystem IsHost() — reliable in both PIE and packaged
    // NM_ListenServer is only set once a client connects, not on session creation
    UNexusMultiplayerSubsystem* Sub = GetSubsystem();
    const bool bIsHost = Sub && Sub->IsHost();

    NEXUS_LOG("[LobbyWidget] SetupForRole — IsHost: %s",
        bIsHost ? TEXT("YES") : TEXT("NO"));

    if (MatchCodePanel)
        MatchCodePanel->SetVisibility(
            bIsHost ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

    if (StartButton)
        StartButton->SetVisibility(
            bIsHost ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

    if (SessionTypeText)
    {
        if (Sub)
            SessionTypeText->SetText(FText::FromString(
                Sub->IsSessionPrivate() ? TEXT("Private") : TEXT("Public")));
    }
}

// ── Button Handlers ───────────────────────────────────────────────────────────

void UNexusLobbyWidget::HandleLeaveClicked()
{
    UNexusMultiplayerSubsystem* Sub = GetSubsystem();
    if (!Sub) return;

    NEXUS_LOG("[LobbyWidget] Leave clicked");
    SetStatus(TEXT("Leaving session..."), FLinearColor::Yellow);

    if (LeaveButton) LeaveButton->SetIsEnabled(false);
    if (StartButton) StartButton->SetIsEnabled(false);

    Sub->LeaveSession();
}

void UNexusLobbyWidget::HandleStartClicked()
{
    UNexusMultiplayerSubsystem* Sub = GetSubsystem();

    if (!Sub || !Sub->IsHost())
    {
        NEXUS_WARN("[LobbyWidget] Start clicked by non-host — ignoring");
        return;
    }

    const FString GameMap = Sub->GetLastSessionConfig().GameMapPath;
    const FString MatchType = Sub->GetLastSessionConfig().MatchType;

    if (GameMap.IsEmpty())
    {
        NEXUS_ERROR("[LobbyWidget] GameMapPath is empty — set it in FNexusSessionConfig");
        SetStatus(TEXT("Error: no game map configured"), FLinearColor::Red);
        return;
    }

    NEXUS_SUCCESS("[LobbyWidget] Host starting game — traveling to: %s", *GameMap);
    SetStatus(TEXT("Starting game..."), FLinearColor::Green);

    if (StartButton) StartButton->SetIsEnabled(false);

    if (INexusOnlineInterface* Online = Sub->GetOnlineLayer())
    {
        Online->SetSessionAttribute(TEXT("MapName"), GameMap);
        Online->SetSessionAttribute(TEXT("MatchType"), MatchType);
    }

    GetWorld()->ServerTravel(GameMap);
}

// ── GameState Delegate Handlers ───────────────────────────────────────────────

void UNexusLobbyWidget::HandleMatchCodeReady(const FString& Code)
{
    NEXUS_SUCCESS("[LobbyWidget] Match code: '%s'", *Code);

    if (MatchCodeText)
    {
        MatchCodeText->SetText(
            FText::FromString(FString::Printf(TEXT("Code: %s"), *Code)));
        MatchCodeText->SetVisibility(ESlateVisibility::Visible);
    }

    if (MatchCodePanel)
        MatchCodePanel->SetVisibility(ESlateVisibility::Visible);
}

void UNexusLobbyWidget::HandlePlayerListUpdated(
    const TArray<FNexusPlayerInfo>& Players)
{
    NEXUS_LOG("[LobbyWidget] Player list updated: %d players", Players.Num());
    RefreshPlayerList(Players);

    if (UNexusMultiplayerSubsystem* Sub = GetSubsystem())
        UpdatePlayerCount(Players.Num(),
            Sub->GetLastSessionConfig().MaxPlayers);
}

// ── Subsystem Delegate Handlers ───────────────────────────────────────────────

void UNexusLobbyWidget::HandleSessionFailed(const FString& Error)
{
    NEXUS_ERROR("[LobbyWidget] Session error: %s", *Error);
    SetStatus(FString::Printf(TEXT("Error: %s"), *Error), FLinearColor::Red);
}

// ── Helpers ───────────────────────────────────────────────────────────────────

void UNexusLobbyWidget::RefreshPlayerList(
    const TArray<FNexusPlayerInfo>& Players)
{
    if (!PlayerListBox) return;

    PlayerListBox->ClearChildren();

    const bool bIsHost = GetWorld()->GetNetMode() == NM_ListenServer;
    const FString LocalId = GetSubsystem()
        ? GetSubsystem()->GetLocalUserId() : FString();

    for (const FNexusPlayerInfo& Player : Players)
    {
        // Build display string: "[Host] PlayerName (You)"
        FString Label = Player.DisplayName;

        if (Player.bIsHost)
            Label = FString::Printf(TEXT("[Host] %s"), *Label);

        const bool bIsLocal = !LocalId.IsEmpty()
            ? Player.UserId == LocalId
            : Player.bIsLocalPlayer;

        if (bIsLocal)
            Label = FString::Printf(TEXT("%s (You)"), *Label);

        if (Player.PingMs > 0 && !bIsHost)
            Label = FString::Printf(TEXT("%s  %dms"), *Label, Player.PingMs);

        UTextBlock* Row = NewObject<UTextBlock>(this);
        Row->SetText(FText::FromString(Label));
        Row->SetColorAndOpacity(FSlateColor(
            bIsLocal ? FLinearColor::Yellow : FLinearColor::White));

        PlayerListBox->AddChild(Row);
    }
}

void UNexusLobbyWidget::SetStatus(const FString& Message, FLinearColor Color)
{
    if (StatusText)
    {
        StatusText->SetText(FText::FromString(Message));
        StatusText->SetColorAndOpacity(FSlateColor(Color));
    }
}

void UNexusLobbyWidget::UpdatePlayerCount(int32 Current, int32 Max)
{
    if (PlayerCountText)
    {
        PlayerCountText->SetText(FText::FromString(
            FString::Printf(TEXT("%d / %d players"), Current, Max)));
    }
}

UNexusMultiplayerSubsystem* UNexusLobbyWidget::GetSubsystem() const
{
    if (Subsystem.IsValid()) return Subsystem.Get();
    return nullptr;
}

ANexusLobbyGameState* UNexusLobbyWidget::GetNexusGameState() const
{
    if (!GetWorld()) return nullptr;
    return GetWorld()->GetGameState<ANexusLobbyGameState>();
}