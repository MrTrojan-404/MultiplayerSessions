// Copyright NexusMultiplayer. All Rights Reserved.

#include "UI/NexusServerRowWidget.h"
#include "Core/NexusLog.h"
#include "Components/TextBlock.h"

void UNexusServerRowWidget::NativeConstruct()
{
	Super::NativeConstruct();
	NEXUS_LOG("[ServerRowWidget] Constructed");
}

void UNexusServerRowWidget::InitRow(const FNexusServerRow& InRow)
{
	RowData = InRow;

	NEXUS_LOG("[ServerRowWidget] InitRow: Host='%s' Players='%s' Ping=%dms Type='%s'",
		*InRow.HostName,
		*InRow.GetPlayerCountString(),
		InRow.PingMs,
		*InRow.MatchType);

	RefreshDisplay();
}

void UNexusServerRowWidget::RefreshDisplay()
{
	if (HostNameText)
		HostNameText->SetText(FText::FromString(RowData.HostName));

	if (PlayerCountText)
		PlayerCountText->SetText(FText::FromString(RowData.GetPlayerCountString()));

	if (PingText)
		PingText->SetText(FText::FromString(RowData.GetPingString()));

	if (MatchTypeText)
		MatchTypeText->SetText(FText::FromString(RowData.MatchType));

	if (MapNameText)
		MapNameText->SetText(FText::FromString(RowData.MapName));
}

void UNexusServerRowWidget::OnRowClicked()
{
	NEXUS_LOG("[ServerRowWidget] Row clicked: SessionId='%s' Host='%s'",
		*RowData.SessionId, *RowData.HostName);

	// Pack the single row back into an array so the delegate signature
	// matches FNexusOnSessionListReady — parent extracts [0]
	OnServerRowClicked.Broadcast({ RowData });
}