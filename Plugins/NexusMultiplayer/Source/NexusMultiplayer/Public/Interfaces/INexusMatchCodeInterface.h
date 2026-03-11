// Copyright NexusMultiplayer. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

class NEXUSMULTIPLAYER_API INexusMatchCodeInterface
{
public:
	virtual ~INexusMatchCodeInterface() = default;

	virtual void    StoreCode(const FString& Code, const FString& SessionId) = 0;
	virtual void    LookupCode(const FString& Code) = 0;
	virtual void    DeleteCode(const FString& Code) = 0;

	virtual FString GenerateCode()
	{
		// No ambiguous chars (no 0/O, 1/I/L)
		static const FString Chars = TEXT("ABCDEFGHJKMNPQRSTUVWXYZ23456789");
		FString Code;
		for (int32 i = 0; i < 6; ++i)
			Code += Chars[FMath::RandRange(0, Chars.Len() - 1)];
		return Code;
	}

	// Delegates — set by NexusMultiplayerSubsystem
	TFunction<void(FString /*Code*/)>      OnCodeStored;
	TFunction<void(FString /*SessionId*/)> OnSessionFound;
	TFunction<void(FString /*Error*/)>     OnCodeError;
};