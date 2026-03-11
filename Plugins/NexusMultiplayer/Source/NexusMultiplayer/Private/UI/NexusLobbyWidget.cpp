// Copyright NexusMultiplayer. All Rights Reserved.

#include "UI/NexusLobbyWidget.h"
#include "Core/NexusLog.h"
#include "Core/NexusMultiplayerSubsystem.h"
#include "GameState//NexusLobbyGameState.h"
#include "Components/TextBlock.h"
#include "Components/ScrollBox.h"
#include "Components/VerticalBox.h"
#include "Components/Button.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

// ── Init ──────────────────────────────────────────────────────────────────────

void UNexusLobbyWidget::NativeConstruct()
{
    Super::NativeConstruct();

    NEXUS_LOG("[LobbyWidget] NativeConstruct");

    UGameInstance* GI = GetGameInstance();
    if (!GI)
    {
        NEXUS_ERROR("[LobbyWidget] GameInstance null");
        return;
    }

    UNexusMultiplayerSubsystem* Sub =
        GI->GetSubsystem<UNexusMultiplayerSubsystem>();
    if (!Sub)
    {
        NEXUS_ERROR("[LobbyWidget] NexusMultiplayerSubsystem not found");
        return;
    }

    Subsystem = Sub;

    // ── Bind buttons ──────────────────────────────────────────────────────

    if (StartButton)
        StartButton->OnClicked.AddDynamic(this, &UNexusLobbyWidget::HandleStartClicked);

    if (LeaveButton)
        LeaveButton->OnClicked.AddDynamic(this, &UNexusLobbyWidget::HandleLeaveClicked);

    // ── Bind subsystem delegates ──────────────────────────────────────────

    Sub->OnMatchCodeReady.AddDynamic(this,  &UNexusLobbyWidget::HandleMatchCodeReady);
    Sub->OnSessionFailed.AddDynamic(this,   &UNexusLobbyWidget::HandleSessionFailed);

    // ── Bind GameState delegates ──────────────────────────────────────────
    // GameState may not be ready yet on clients — retry via a short timer

    BindGameStateDelegates();

    // ── Role-specific setup ───────────────────────────────────────────────

    SetupForRole();

    // Show any already-stored match code (host travels before code is ready)
    const FString ExistingCode = Sub->GetActiveMatchCode();
    if (!ExistingCode.IsEmpty())
    {
        NEXUS_LOG("[LobbyWidget] Match code already available: '%s'", *ExistingCode);
        HandleMatchCodeReady(ExistingCode);
    }

    NEXUS_SUCCESS("[LobbyWidget] Init complete. IsHost=%s",
        Sub->IsHost() ? TEXT("YES") : TEXT("NO"));
}

void UNexusLobbyWidget::NativeDestruct()
{
    if (UNexusMultiplayerSubsystem* Sub = GetSubsystem())
    {
        Sub->OnMatchCodeReady.RemoveDynamic(this, &UNexusLobbyWidget::HandleMatchCodeReady);
        Sub->OnSessionFailed.RemoveDynamic(this,  &UNexusLobbyWidget::HandleSessionFailed);
    }

    if (UWorld* World = GetWorld())
    {
        if (ANexusLobbyGameState* GS =
            World->GetGameState<ANexusLobbyGameState>())
        {
            GS->OnPlayerListUpdated.RemoveDynamic(
                this, &UNexusLobbyWidget::HandlePlayerListUpdated);
            GS->OnMatchCodeReady.RemoveDynamic(
                this, &UNexusLobbyWidget::HandleMatchCodeReady);
        }
    }

    NEXUS_LOG("[LobbyWidget] NativeDestruct — delegates unbound");
    Super::NativeDestruct();
}

// ── Role Setup ────────────────────────────────────────────────────────────────

void UNexusLobbyWidget::SetupForRole()
{
    UNexusMultiplayerSubsystem* Sub = GetSubsystem();
    if (!Sub) return;

    const bool bIsHost = Sub->IsHost();

    // Show match code panel only for host
    if (MatchCodePanel)
        MatchCodePanel->SetVisibility(
            bIsHost ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

    // Only host can start the game
    if (StartButton)
    {
        StartButton->SetVisibility(
            bIsHost ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
        StartButton->SetIsEnabled(bIsHost);
    }

    SetStatus(bIsHost
        ? TEXT("Waiting for players...")
        : TEXT("Waiting for host to start..."));
}

// ── Button Handlers ───────────────────────────────────────────────────────────

void UNexusLobbyWidget::HandleStartClicked()
{
    UNexusMultiplayerSubsystem* Sub = GetSubsystem();
    if (!Sub || !Sub->IsHost())
    {
        NEXUS_WARN("[LobbyWidget] Start clicked but not host — ignoring");
        return;
    }

    NEXUS_LOG("[LobbyWidget] Start clicked — host traveling to game map: '%s'",
        *GameMapPath);

    SetStatus(TEXT("Starting game..."), FLinearColor::Green);

    if (StartButton)
        StartButton->SetIsEnabled(false);

    // ServerTravel broadcasts to all connected clients automatically
    UWorld* World = GetWorld();
    if (!World)
    {
        NEXUS_ERROR("[LobbyWidget] Start: World is null");
        return;
    }

    World->ServerTravel(GameMapPath);
}

void UNexusLobbyWidget::HandleLeaveClicked()
{
    UNexusMultiplayerSubsystem* Sub = GetSubsystem();
    if (!Sub) return;

    NEXUS_LOG("[LobbyWidget] Leave clicked");
    SetStatus(TEXT("Leaving..."), FLinearColor::Yellow);

    Sub->LeaveSession();
}

// ── Delegate Handlers ─────────────────────────────────────────────────────────

void UNexusLobbyWidget::HandleMatchCodeReady(const FString& Code)
{
    NEXUS_SUCCESS("[LobbyWidget] Match code: '%s'", *Code);

    if (MatchCodeText)
        MatchCodeText->SetText(FText::FromString(Code));
}

void UNexusLobbyWidget::HandlePlayerListUpdated(
    const TArray<FNexusPlayerInfo>& Players)
{
    NEXUS_LOG("[LobbyWidget] Player list updated: %d players", Players.Num());
    RefreshPlayerList(Players);
}

void UNexusLobbyWidget::HandleSessionFailed(const FString& Error)
{
    NEXUS_ERROR("[LobbyWidget] Session error: %s", *Error);
    SetStatus(Error, FLinearColor::Red);
}

// ── Helpers ───────────────────────────────────────────────────────────────────

void UNexusLobbyWidget::BindGameStateDelegates()
{
    UWorld* World = GetWorld();
    if (!World) return;

    ANexusLobbyGameState* GS =
        World->GetGameState<ANexusLobbyGameState>();

    if (!GS)
    {
        // GameState not replicated yet on clients — retry next frame
        NEXUS_WARN("[LobbyWidget] GameState not available yet — will retry binding");

        FTimerHandle Handle;
        World->GetTimerManager().SetTimer(Handle, [this]()
        {
            BindGameStateDelegates();
        }, 0.2f, false);
        return;
    }

    GS->OnPlayerListUpdated.AddDynamic(
        this, &UNexusLobbyWidget::HandlePlayerListUpdated);
    GS->OnMatchCodeReady.AddDynamic(
        this, &UNexusLobbyWidget::HandleMatchCodeReady);

    NEXUS_LOG("[LobbyWidget] Bound to ANexusLobbyGameState delegates");

    // Populate immediately with whatever is already replicated
    const TArray<FNexusPlayerInfo>& Players = GS->GetPlayerList();
    if (Players.Num() > 0)
        RefreshPlayerList(Players);

    const FString& ExistingCode = GS->GetMatchCode();
    if (!ExistingCode.IsEmpty())
        HandleMatchCodeReady(ExistingCode);
}

void UNexusLobbyWidget::RefreshPlayerList(const TArray<FNexusPlayerInfo>& Players)
{
    if (!PlayerListBox) return;

    PlayerListBox->ClearChildren();

    for (const FNexusPlayerInfo& Player : Players)
    {
        // Simple text row — replace with a proper PlayerRowWidget if desired
        UTextBlock* NameText = NewObject<UTextBlock>(this);

        FString Label = Player.DisplayName;
        if (Player.bIsHost)    Label += TEXT(" [Host]");
        if (Player.bIsLocalPlayer) Label += TEXT(" (You)");

        NameText->SetText(FText::FromString(Label));
        PlayerListBox->AddChild(NameText);
    }

    if (PlayerCountText)
    {
        // Get max from subsystem's last session config if available
        const FString CountStr = FString::Printf(TEXT("%d players"), Players.Num());
        PlayerCountText->SetText(FText::FromString(CountStr));
    }

    NEXUS_LOG("[LobbyWidget] Refreshed player list: %d entries", Players.Num());
}

UNexusMultiplayerSubsystem* UNexusLobbyWidget::GetSubsystem() const
{
    if (Subsystem.IsValid()) return Subsystem.Get();
    return nullptr;
}

void UNexusLobbyWidget::SetStatus(const FString& Message, FLinearColor Color)
{
    if (StatusText)
    {
        StatusText->SetText(FText::FromString(Message));
        StatusText->SetColorAndOpacity(FSlateColor(Color));
    }
}