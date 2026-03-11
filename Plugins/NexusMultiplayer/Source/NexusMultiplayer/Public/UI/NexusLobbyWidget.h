// Copyright NexusMultiplayer. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/NexusTypes.h"
#include "NexusLobbyWidget.generated.h"

class UNexusMultiplayerSubsystem;
class ANexusLobbyGameState;
class UTextBlock;
class UScrollBox;
class UButton;
class UVerticalBox;

/**
 * UNexusLobbyWidget
 *
 * Lobby screen shown to all players after traveling to the lobby map.
 * Shows the match code (host only), player list, and a ready/start button.
 *
 * In your UMG Blueprint subclass bind:
 *   MatchCodeText       — shows the 6-char code (only visible to host)
 *   MatchCodePanel      — the whole code display panel (hidden for clients)
 *   PlayerListBox       — VerticalBox populated with player name rows
 *   PlayerCountText     — "2 / 4"
 *   StartButton         — only visible + enabled for host
 *   LeaveButton         — always visible
 *   StatusText          — shows waiting message or errors
 */
UCLASS(Abstract)
class NEXUSMULTIPLAYER_API UNexusLobbyWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // ── UMG Bindings ─────────────────────────────────────────────────────────

    // Shows the match code — host only
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> MatchCodeText;

    // The whole panel containing the code — hide for clients
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<class UWidget> MatchCodePanel;

    // "2 / 4"
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> PlayerCountText;

    // Rows of player names
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UVerticalBox> PlayerListBox;

    // Only host can click this
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> StartButton;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> LeaveButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> StatusText;

    // ── Config ────────────────────────────────────────────────────────────────

    // Path the host travels to when clicking Start
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nexus|Config")
    FString GameMapPath = TEXT("/Game/Maps/GameMap");

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

private:
    TWeakObjectPtr<UNexusMultiplayerSubsystem> Subsystem;
    UNexusMultiplayerSubsystem* GetSubsystem() const;

    // ── Button handlers ───────────────────────────────────────────────────────

    UFUNCTION()
    void HandleStartClicked();

    UFUNCTION()
    void HandleLeaveClicked();

    // ── Subsystem / GameState delegate handlers ───────────────────────────────

    UFUNCTION()
    void HandleMatchCodeReady(const FString& Code);

    UFUNCTION()
    void HandlePlayerListUpdated(const TArray<FNexusPlayerInfo>& Players);

    UFUNCTION()
    void HandleSessionFailed(const FString& Error);

    // ── Helpers ───────────────────────────────────────────────────────────────

    void BindGameStateDelegates();
    void RefreshPlayerList(const TArray<FNexusPlayerInfo>& Players);
    void SetStatus(const FString& Message, FLinearColor Color = FLinearColor::White);

    // Called once after NativeConstruct to set initial host/client state
    void SetupForRole();
};