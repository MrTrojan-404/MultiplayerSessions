// Copyright NexusMultiplayer. All Rights Reserved.

#include "Backend/Nakama/NexusNakamaCallbackProxy.h"
#include "Backend/Nakama/NexusNakamaBackend.h"
#include "MatchCode/NexusNakamaMatchCode.h"

// ── Backend proxy ─────────────────────────────────────────────────────────────

void UNexusNakamaBackendProxy::OnAuthSuccess(UNakamaSession* Session)
{
	if (Owner) Owner->HandleAuthSuccess(Session);
}

void UNexusNakamaBackendProxy::OnAuthError(const FNakamaError& Error)
{
	if (Owner) Owner->HandleAuthError(Error);
}

void UNexusNakamaBackendProxy::OnGetAccountSuccess(const FNakamaAccount& Account)
{
	if (Owner) Owner->HandleGetAccountSuccess(Account);
}

void UNexusNakamaBackendProxy::OnGetAccountError(const FNakamaError& Error)
{
	if (Owner) Owner->HandleGetAccountError(Error);
}

void UNexusNakamaBackendProxy::OnUpdateAccountSuccess()
{
	if (Owner) Owner->HandleSetDisplayNameSuccess();
}

void UNexusNakamaBackendProxy::OnUpdateAccountError(const FNakamaError& Error)
{
	if (Owner) Owner->HandleSetDisplayNameError(Error);
}

// ── Match proxy ───────────────────────────────────────────────────────────────

void UNexusNakamaMatchProxy::OnStoreSuccess(const FNakamaStoreObjectAcks& Acks)
{
	if (Owner) Owner->HandleStoreSuccess(Acks);
}

void UNexusNakamaMatchProxy::OnStoreError(const FNakamaError& Error)
{
	if (Owner) Owner->HandleStoreError(Error);
}

void UNexusNakamaMatchProxy::OnLookupSuccess(const FNakamaStorageObjectList& Objects)
{
	if (Owner) Owner->HandleLookupSuccess(Objects);
}

void UNexusNakamaMatchProxy::OnLookupError(const FNakamaError& Error)
{
	if (Owner) Owner->HandleLookupError(Error);
}

void UNexusNakamaMatchProxy::OnDeleteError(const FNakamaError& Error)
{
	if (Owner) Owner->HandleDeleteError(Error);
}

void UNexusNakamaMatchProxy::OnDeleteSuccess()
{
	if (Owner) Owner->HandleDeleteSuccess();
}

void UNexusNakamaBackendProxy::OnWriteLeaderboardSuccess(const FNakamaLeaderboardRecord& Record)
{
	if (Owner) Owner->HandleWriteLeaderboardSuccess(Record);
}
 
void UNexusNakamaBackendProxy::OnWriteLeaderboardError(const FNakamaError& Error)
{
	if (Owner) Owner->HandleWriteLeaderboardError(Error);
}
 
void UNexusNakamaBackendProxy::OnListLeaderboardSuccess(const FNakamaLeaderboardRecordList& RecordList)
{
	if (Owner) Owner->HandleListLeaderboardSuccess(RecordList);
}
 
void UNexusNakamaBackendProxy::OnListLeaderboardError(const FNakamaError& Error)
{
	if (Owner) Owner->HandleListLeaderboardError(Error);
}
 