// Copyright NexusMultiplayer. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/NexusTypes.h"
#include "NexusLobbyWidget.generated.h"

class UNexusMultiplayerSubsystem;
class ANexusLobbyGameState;
class UButton;
class UTextBlock;
class UVerticalBox;

/**
 * UNexusLobbyWidget
 *
 * Shown while players wait in the lobby before the game starts.
 * Displays the player list, match code (host only), and a Start button (host only).
 *
 * Required UMG bindings:
 *   PlayerListBox, LeaveButton
 *
 * Optional UMG bindings:
 *   MatchCodePanel, MatchCodeText, StartButton,
 *   PlayerCountText, StatusText, SessionTypeText
 *
 * Add to viewport in your Lobby map's Level Blueprint or PlayerController BeginPlay.
 */
UCLASS(Abstract)
class NEXUSMULTIPLAYER_API UNexusLobbyWidget : public UUserWidget
{
    GENERATED_BODY()

public:

    // ── Required bindings ─────────────────────────────────────────────────────

    // Vertical box — rows added here per player
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UVerticalBox> PlayerListBox;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> LeaveButton;

    // ── Optional bindings ─────────────────────────────────────────────────────

    // Whole panel containing the code — hidden for clients
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UWidget> MatchCodePanel;

    // Shows the match code e.g. "Code: XK39QW"
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> MatchCodeText;

    // Only visible and functional on the host
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> StartButton;

    // "2 / 4 players"
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> PlayerCountText;

    // General status messages
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> StatusText;

    // "Public" or "Private"
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> SessionTypeText;

protected:

    virtual void NativeConstruct() override;
    virtual void NativeDestruct()  override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:

    // ── Refs ──────────────────────────────────────────────────────────────────

    TWeakObjectPtr<UNexusMultiplayerSubsystem> Subsystem;
    TWeakObjectPtr<ANexusLobbyGameState>       GameState;

    UNexusMultiplayerSubsystem* GetSubsystem() const;
    ANexusLobbyGameState*       GetNexusGameState() const;

    // ── Setup ─────────────────────────────────────────────────────────────────

    // Called once GameState is available — retries on tick if not ready yet
    void TryBindGameState();
    bool bGameStateBound = false;

    // Configures visibility of host-only elements
    void SetupForRole();

    // ── Button handlers ───────────────────────────────────────────────────────

    UFUNCTION() void HandleLeaveClicked();
    UFUNCTION() void HandleStartClicked();

    // ── GameState delegate handlers ───────────────────────────────────────────

    UFUNCTION() void HandleMatchCodeReady(const FString& Code);
    UFUNCTION() void HandlePlayerListUpdated(const TArray<FNexusPlayerInfo>& Players);

    // ── Subsystem delegate handlers ───────────────────────────────────────────

    UFUNCTION() void HandleSessionFailed(const FString& Error);

    // ── Helpers ───────────────────────────────────────────────────────────────

    void RefreshPlayerList(const TArray<FNexusPlayerInfo>& Players);
    void SetStatus(const FString& Message,
        FLinearColor Color = FLinearColor::White);
    void UpdatePlayerCount(int32 Current, int32 Max);
};