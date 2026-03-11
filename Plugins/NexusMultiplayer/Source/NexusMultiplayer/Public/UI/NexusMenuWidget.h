// Copyright NexusMultiplayer. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/NexusTypes.h"
#include "NexusMenuWidget.generated.h"

class UNexusMultiplayerSubsystem;
class UNexusServerRowWidget;
class UButton;
class UEditableTextBox;
class UScrollBox;
class UTextBlock;
class UWidgetSwitcher;

/**
 * UNexusMenuWidget
 *
 * Main menu widget. Handles host, find, join-by-code, and server browser.
 *
 * In your UMG Blueprint subclass bind:
 *   Buttons:   HostButton, FindButton, JoinByCodeButton, RefreshButton
 *   Inputs:    CodeInputBox, MaxPlayersBox, MatchTypeBox, LobbyMapBox
 *   Display:   DisplayNameText, StatusText, MatchCodeDisplay
 *   Lists:     ServerListScrollBox
 *   Switcher:  MenuSwitcher (0=Main, 1=ServerBrowser, 2=Hosting, 3=Loading)
 *
 * Set ServerRowWidgetClass to your UNexusServerRowWidget Blueprint subclass.
 */
UCLASS(Abstract)
class NEXUSMULTIPLAYER_API UNexusMenuWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // ── UMG Bindings ─────────────────────────────────────────────────────────

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> HostButton;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> FindButton;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> JoinByCodeButton;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> RefreshButton;

    // Code input for join-by-code
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UEditableTextBox> CodeInputBox;

    // Optional session config inputs — leave blank for defaults
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UEditableTextBox> MaxPlayersBox;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UEditableTextBox> MatchTypeBox;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UEditableTextBox> LobbyMapBox;

    // Private session checkbox
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<class UCheckBox> PrivateCheckBox;

    // Server list container
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UScrollBox> ServerListScrollBox;

    // Status / error display
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> StatusText;

    // Local player's display name
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> DisplayNameText;

    // Shows the generated match code after hosting a private session
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> MatchCodeDisplay;

    // 0=Main 1=ServerBrowser 2=Hosting 3=Loading
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UWidgetSwitcher> MenuSwitcher;

    // ── Config ────────────────────────────────────────────────────────────────

    // Assign your UNexusServerRowWidget Blueprint subclass here in the editor
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nexus|Config",
        meta = (DisplayName = "Server Row Widget Class"))
    TSubclassOf<UNexusServerRowWidget> ServerRowWidgetClass;

    // Default session config — overridden by input box values if present
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nexus|Config",
        meta = (DisplayName = "Default Session Config"))
    FNexusSessionConfig DefaultSessionConfig;

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

private:
    // ── Subsystem ─────────────────────────────────────────────────────────────

    TWeakObjectPtr<UNexusMultiplayerSubsystem> Subsystem;
    UNexusMultiplayerSubsystem* GetSubsystem() const;

    // ── Button handlers ───────────────────────────────────────────────────────

    UFUNCTION()
    void HandleHostClicked();

    UFUNCTION()
    void HandleFindClicked();

    UFUNCTION()
    void HandleJoinByCodeClicked();

    UFUNCTION()
    void HandleRefreshClicked();

    // ── Subsystem delegate handlers ───────────────────────────────────────────

    UFUNCTION()
    void HandleSessionCreated(bool bWasSuccessful);

    UFUNCTION()
    void HandleSessionJoined(bool bWasSuccessful);

    UFUNCTION()
    void HandleSessionFailed(const FString& Error);

    UFUNCTION()
    void HandleSessionListReady(const TArray<FNexusServerRow>& Rows);

    UFUNCTION()
    void HandleMatchCodeReady(const FString& Code);

    UFUNCTION()
    void HandleDisplayNameReady(const FString& Name);

    // ── Server row handler ────────────────────────────────────────────────────

    UFUNCTION()
    void HandleServerRowClicked(const TArray<FNexusServerRow>& Rows);

    // ── Helpers ───────────────────────────────────────────────────────────────

    void SetStatus(const FString& Message, FLinearColor Color = FLinearColor::White);
    void SetLoadingState(bool bLoading);
    void ClearServerList();
    void PopulateServerList(const TArray<FNexusServerRow>& Rows);
    FNexusSessionConfig BuildSessionConfig() const;
    void ShowPanel(int32 Index);
};