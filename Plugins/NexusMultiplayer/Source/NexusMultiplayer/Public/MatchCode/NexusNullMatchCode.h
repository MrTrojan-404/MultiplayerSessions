// Copyright NexusMultiplayer. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Core/NexusLog.h"
#include "Interfaces/INexusMatchCodeInterface.h"

/**
 * FNexusNullMatchCode
 * In-memory match code store — LAN / dev / PIE use only.
 * No persistence, lives only for the duration of the process.
 */
class NEXUSMULTIPLAYER_API FNexusNullMatchCode : public INexusMatchCodeInterface
{
public:
	virtual void StoreCode(const FString& Code, const FString& SessionId) override
	{
		NEXUS_LOG("[NullMatchCode] StoreCode: '%s' → '%s'", *Code, *SessionId);
		CodeMap.Add(Code, SessionId);
		if (OnCodeStored) OnCodeStored(Code);
	}

	virtual void LookupCode(const FString& Code) override
	{
		NEXUS_LOG("[NullMatchCode] LookupCode: '%s'", *Code);
		const FString* Found = CodeMap.Find(Code);
		if (Found)
		{
			NEXUS_SUCCESS("[NullMatchCode] Found SessionId: '%s'", **Found);
			if (OnSessionFound) OnSessionFound(*Found);
		}
		else
		{
			NEXUS_WARN("[NullMatchCode] Code '%s' not found in memory store", *Code);
			if (OnCodeError) OnCodeError(TEXT("Match code not found"));
		}
	}

	virtual void DeleteCode(const FString& Code) override
	{
		NEXUS_LOG("[NullMatchCode] DeleteCode: '%s'", *Code);
		CodeMap.Remove(Code);
	}

private:
	TMap<FString, FString> CodeMap; // Code → SessionId
};