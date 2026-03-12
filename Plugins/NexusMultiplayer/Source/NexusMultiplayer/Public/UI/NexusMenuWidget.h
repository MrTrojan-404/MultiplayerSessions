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
class UCheckBox;

/**
 * UNexusMenuWidget
 *
 * Full-screen menu widget. The MenuSwitcher IS the root — it fills the entire
 * canvas and swaps between four full-screen panels:
 *
 *   0 = Main      — Host / Find / JoinByCode
 *   1 = Browser   — Server list results
 *   2 = Hosting   — Session config inputs before hosting
 *   3 = Loading   — Any async operation in progress
 *
 * Required UMG bindings (exact names):
 *   HostButton, FindButton, JoinByCodeButton, RefreshButton,
 *   ConfirmHostButton, BackButton,
 *   CodeInputBox, ServerListScrollBox, StatusText
 *
 * Optional UMG bindings:
 *   MaxPlayersBox, MatchTypeBox, LobbyMapBox, PrivateCheckBox,
 *   DisplayNameText, MatchCodeDisplay, MenuSwitcher
 *
 * Blueprint defaults to set:
 *   ServerRowWidgetClass — your WBP_NexusServerRow subclass
 *   DefaultSessionConfig — fallback values if inputs are blank
 */
UCLASS(Abstract)
class NEXUSMULTIPLAYER_API UNexusMenuWidget : public UUserWidget
{
    GENERATED_BODY()

public:

    // ── Required bindings ─────────────────────────────────────────────────────

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> HostButton;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> FindButton;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> JoinByCodeButton;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> RefreshButton;

    // On Panel_Hosting — confirms config and starts session
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> ConfirmHostButton;

    // On Panel_Browser / Panel_Hosting — returns to Main
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> BrowserBackButton;   // on Panel_Browser

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> HostingBackButton;
    
    // Code input for join-by-code
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UEditableTextBox> CodeInputBox;

    // Server results list (Panel_Browser)
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UScrollBox> ServerListScrollBox;

    // Status / error messages (Panel_Loading + Panel_Browser)
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> StatusText;

    // ── Optional bindings ─────────────────────────────────────────────────────

    // Session config inputs on Panel_Hosting
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UEditableTextBox> MaxPlayersBox;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<class UComboBoxString> MatchTypeCombo;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UComboBoxString> MapCombo;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UCheckBox> PrivateCheckBox;

    // Logged-in player name (Panel_Main top bar)
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> DisplayNameText;

    // Root panel switcher — covers full screen, swaps between 4 panels
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UWidgetSwitcher> MenuSwitcher;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UCheckBox> LANCheckBox;

    // ── Config ────────────────────────────────────────────────────────────────

    // Assign your WBP_NexusServerRow Blueprint subclass here
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nexus|Config")
    TSubclassOf<UNexusServerRowWidget> ServerRowWidgetClass;

    // Fallback values — overridden by input box values when filled
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nexus|Config")
    FNexusSessionConfig DefaultSessionConfig;

    // Set these in Blueprint defaults — defines what maps appear in the picker
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nexus|Config")
    TArray<FNexusMapEntry> AvailableMaps;
protected:

    virtual void NativeConstruct() override;
    virtual void NativeDestruct()  override;

private:

    // ── Subsystem ref ─────────────────────────────────────────────────────────

    TWeakObjectPtr<UNexusMultiplayerSubsystem> Subsystem;
    UNexusMultiplayerSubsystem* GetSubsystem() const;

    // ── Button handlers ───────────────────────────────────────────────────────

    UFUNCTION() void HandleHostClicked();
    UFUNCTION() void HandleFindClicked();
    UFUNCTION() void HandleJoinByCodeClicked();
    UFUNCTION() void HandleRefreshClicked();
    UFUNCTION() void HandleConfirmHostClicked();
    UFUNCTION() void HandleBackClicked();

    // ── Subsystem delegate handlers ───────────────────────────────────────────

    UFUNCTION() void HandleSessionCreated(bool bWasSuccessful);
    UFUNCTION() void HandleSessionJoined(bool bWasSuccessful);
    UFUNCTION() void HandleSessionFailed(const FString& Error);
    UFUNCTION() void HandleSessionListReady(const TArray<FNexusServerRow>& Rows);
    UFUNCTION() void HandleDisplayNameReady(const FString& Name);

    // ── Server row handler ────────────────────────────────────────────────────

    UFUNCTION() void HandleServerRowClicked(const TArray<FNexusServerRow>& Rows);

    // ── Panel helpers ─────────────────────────────────────────────────────────

    // Panels: 0=Main 1=Browser 2=Hosting 3=Loading
    void ShowMain();
    void ShowBrowser();
    void ShowHosting();
    void ShowLoading(const FString& Message);

    // ── Other helpers ─────────────────────────────────────────────────────────

    void SetStatus(const FString& Message, FLinearColor Color = FLinearColor::White);
    void SetButtonsEnabled(bool bEnabled);
    void ClearServerList();
    void PopulateServerList(const TArray<FNexusServerRow>& Rows);
    FNexusSessionConfig BuildSessionConfig() const;

    // Tracks whichever map the player selected on Panel_Hosting
    FNexusMapEntry SelectedMap;

    UFUNCTION()
    void HandleMapSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

    UFUNCTION()
    void HandleLANCheckboxChanged(bool bIsChecked);
};