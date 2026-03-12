// Copyright NexusMultiplayer. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/NexusTypes.h"
#include "NexusServerRowWidget.generated.h"

/**
 * UNexusServerRowWidget
 *
 * One row in the server browser list.
 * Populated by UNexusMenuWidget when FindSessions results arrive.
 *
 * In your UMG Blueprint subclass, bind the four text blocks to:
 *   HostNameText, PlayerCountText, PingText, MatchTypeText
 *
 * The row fires OnServerRowClicked when clicked so the parent
 * menu widget can call JoinSessionFromRow without knowing internals.
 */
UCLASS(Abstract)
class NEXUSMULTIPLAYER_API UNexusServerRowWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // Called by UNexusMenuWidget after spawning this row
    UFUNCTION(BlueprintCallable, Category = "Nexus|ServerRow")
    void InitRow(const FNexusServerRow& InRow);

    // The row data this widget represents
    UFUNCTION(BlueprintPure, Category = "Nexus|ServerRow")
    const FNexusServerRow& GetRowData() const { return RowData; }

    // Parent menu binds this to know which row was clicked
    UPROPERTY(BlueprintAssignable, Category = "Nexus|Events")
    FNexusOnSessionListReady OnServerRowClicked;

    // ── UMG Bindings — name these exactly in your Blueprint ──────────────────

    // Host display name
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<class UTextBlock> HostNameText;

    // "2 / 4"
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<class UTextBlock> PlayerCountText;

    // "42 ms"
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<class UTextBlock> PingText;

    // "FreeForAll"
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<class UTextBlock> MatchTypeText;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> MapNameText;

    // Click handler — wire this to your UMG button's OnClicked
    UFUNCTION(BlueprintCallable, Category = "Nexus|ServerRow")
    void OnRowClicked();

protected:
    virtual void NativeConstruct() override;

private:
    FNexusServerRow RowData;

    void RefreshDisplay();
};