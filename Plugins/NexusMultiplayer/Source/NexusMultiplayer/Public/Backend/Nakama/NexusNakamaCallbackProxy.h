// Copyright NexusMultiplayer. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "NakamaSession.h"
#include "NakamaError.h"
#include "NakamaAccount.h"
#include "NakamaStorageObject.h"
#include "NakamaLeaderboard.h"
#include "NexusNakamaCallbackProxy.generated.h"

class UNakamaClient;
class FNexusNakamaBackend;
class FNexusNakamaMatchCode;

/**
 * UNexusNakamaBackendProxy
 *
 * Thin UObject shim so FNexusNakamaBackend (plain C++) can bind to
 * Nakama's DYNAMIC_MULTICAST delegates which require a UObject target.
 * Lifetime is tied to the backend — backend creates and owns it.
 */
UCLASS()
class NEXUSMULTIPLAYER_API UNexusNakamaBackendProxy : public UObject
{
	GENERATED_BODY()

public:
	// Set by FNexusNakamaBackend immediately after NewObject<>
	FNexusNakamaBackend* Owner = nullptr;

	UPROPERTY()
	UNakamaSession* HeldSession = nullptr;
	
	UFUNCTION()
	void OnAuthSuccess(UNakamaSession* Session);

	UFUNCTION()
	void OnAuthError(const FNakamaError& Error);

	UFUNCTION()
	void OnGetAccountSuccess(const FNakamaAccount& Account);

	UFUNCTION()
	void OnGetAccountError(const FNakamaError& Error);

	UFUNCTION()
	void OnUpdateAccountSuccess();

	UFUNCTION()
	void OnUpdateAccountError(const FNakamaError& Error);

	UFUNCTION()
	void OnWriteLeaderboardSuccess(const FNakamaLeaderboardRecord& Record);

	UFUNCTION()
	void OnWriteLeaderboardError(const FNakamaError& Error);

	UFUNCTION()
	void OnListLeaderboardSuccess(const FNakamaLeaderboardRecordList& RecordList);

	UFUNCTION()
	void OnListLeaderboardError(const FNakamaError& Error);
};

/**
 * UNexusNakamaMatchProxy
 *
 * Same pattern for FNexusNakamaMatchCode storage callbacks.
 */
UCLASS()
class NEXUSMULTIPLAYER_API UNexusNakamaMatchProxy : public UObject
{
	GENERATED_BODY()

public:
	FNexusNakamaMatchCode* Owner = nullptr;

	UPROPERTY()
	UNakamaSession* HeldSession = nullptr;

	UPROPERTY()
	UNakamaClient* HeldClient = nullptr;
	
	UFUNCTION()
	void OnStoreSuccess(const FNakamaStoreObjectAcks& Acks);

	UFUNCTION()
	void OnStoreError(const FNakamaError& Error);

	UFUNCTION()
	void OnLookupSuccess(const FNakamaStorageObjectList& Objects);

	UFUNCTION()
	void OnLookupError(const FNakamaError& Error);

	UFUNCTION()
	void OnDeleteError(const FNakamaError& Error);

	UFUNCTION()
	void OnDeleteSuccess();
};