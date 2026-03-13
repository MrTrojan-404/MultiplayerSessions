// Copyright NexusMultiplayer. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Backend/Nakama/NexusNakamaCallbackProxy.h"
#include "Interfaces/INexusMatchCodeInterface.h"

class UNexusNakamaMatchProxy;
class UNakamaClient;
class UNakamaSession;
struct FNakamaError;
struct FNakamaStoreObjectAcks;
struct FNakamaStorageObjectList;

/**
 * FNexusNakamaMatchCode
 *
 * Match code storage via Nakama storage objects.
 *
 * StoreCode  → writes {"code": Code, "sessionId": SessionId}
 *              to collection "match_codes", key = Code
 *              with PUBLIC read so any client can look it up
 *
 * LookupCode → reads collection "match_codes", key = Code
 *              on success fires OnSessionFound(SessionId)
 *              on miss fires OnCodeError
 *
 * DeleteCode → deletes the storage object (host cleanup)
 *
 * Tradeoffs:
 *   + Survives host crash (server-side storage)
 *   + Cross-platform (Steam + EOS clients can share codes)
 *   - Requires Nakama server to be reachable
 *   - ~100-300ms async roundtrip vs Steam's instant lobby write
 */
class NEXUSMULTIPLAYER_API FNexusNakamaMatchCode : public INexusMatchCodeInterface
{
public:
    FNexusNakamaMatchCode(UNakamaClient* InClient, UNakamaSession* InSession);
    ~FNexusNakamaMatchCode();

    virtual void StoreCode(const FString& Code, const FString& SessionId) override;
    virtual void LookupCode(const FString& Code) override;
    virtual void DeleteCode(const FString& Code) override;

    // Called by NexusMultiplayerSubsystem when session refreshes
    void UpdateSession(UNakamaSession* NewSession)
    {
        Session = NewSession;
        if (Proxy) Proxy->HeldSession = NewSession;
    }
    // Storage callbacks
    void HandleStoreSuccess(const FNakamaStoreObjectAcks& Acks);
    void HandleStoreError(const FNakamaError& Error);

    void HandleLookupSuccess(const FNakamaStorageObjectList& Objects);
    void HandleLookupError(const FNakamaError& Error);

    void HandleDeleteSuccess();
    void HandleDeleteError(const FNakamaError& Error);

private:
    UNakamaClient*  Client  = nullptr;
    UNakamaSession* Session = nullptr;

    static const FString Collection;
    static const FString LookupUserId; // server-owned records use empty string owner

    // Pending code tracked for lookup response
    FString PendingLookupCode;

    bool EnsureClientAndSession(const FString& CallerName) const;

    TObjectPtr<UNexusNakamaMatchProxy> Proxy;

    

};