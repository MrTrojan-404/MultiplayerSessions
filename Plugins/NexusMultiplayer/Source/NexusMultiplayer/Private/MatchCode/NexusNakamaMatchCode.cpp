// Copyright NexusMultiplayer. All Rights Reserved.

#include "MatchCode/NexusNakamaMatchCode.h"
#include "Core/NexusLog.h"
#include "NakamaClient.h"
#include "NakamaSession.h"
#include "NakamaStorageObject.h"
#include "Backend/Nakama/NexusNakamaCallbackProxy.h"

const FString FNexusNakamaMatchCode::Collection   = TEXT("match_codes");
const FString FNexusNakamaMatchCode::LookupUserId = TEXT("");

// ── Constructor ───────────────────────────────────────────────────────────────

FNexusNakamaMatchCode::FNexusNakamaMatchCode(
    UNakamaClient*  InClient,
    UNakamaSession* InSession)
    : Client(InClient)
    , Session(InSession)
{
    NEXUS_LOG("[NakamaMatchCode] Created. Client valid: %s | Session valid: %s",
        Client  ? TEXT("YES") : TEXT("NO"),
        Session ? TEXT("YES") : TEXT("NO"));

    Proxy = NewObject<UNexusNakamaMatchProxy>(GetTransientPackage());
    Proxy->Owner = this;
    Proxy->AddToRoot();

    Proxy->HeldSession = InSession;
    Proxy->HeldClient  = InClient;    

    NEXUS_LOG("[NakamaMatchCode] Created.");
}

FNexusNakamaMatchCode::~FNexusNakamaMatchCode()
{
    if (Proxy)
    {
        Proxy->HeldSession = nullptr;
        Proxy->HeldClient  = nullptr;
        Proxy->Owner = nullptr;
        Proxy->RemoveFromRoot();
        Proxy = nullptr;
    }
}

// ── StoreCode ─────────────────────────────────────────────────────────────────

void FNexusNakamaMatchCode::StoreCode(
    const FString& Code, const FString& SessionId)
{
    if (!EnsureClientAndSession(TEXT("StoreCode"))) return;

    NEXUS_LOG("[NakamaMatchCode] StoreCode: Code='%s' SessionId='%s'",
        *Code, *SessionId);

    // JSON payload: { "sessionId": "..." }
    // Key = the match code itself — makes direct lookup by key possible
    const FString Payload = FString::Printf(
        TEXT("{\"sessionId\":\"%s\"}"), *SessionId);

    FNakamaStoreObjectWrite WriteObj;
    WriteObj.Collection     = Collection;
    WriteObj.Key            = Code;
    WriteObj.Value          = Payload;
    WriteObj.PermissionRead = ENakamaStoragePermissionRead::PUBLIC_READ;
    WriteObj.PermissionWrite = ENakamaStoragePermissionWrite::OWNER_WRITE;

    FOnStorageObjectAcks Success;
    Success.AddDynamic(Proxy, &UNexusNakamaMatchProxy::OnStoreSuccess);

    FOnError Error;
    Error.AddDynamic(Proxy, &UNexusNakamaMatchProxy::OnStoreError);

    Client->WriteStorageObjects(Session, {WriteObj}, Success, Error);
}

void FNexusNakamaMatchCode::HandleStoreSuccess(const FNakamaStoreObjectAcks& Acks)
{
    if (Acks.StorageObjects.Num() > 0)
    {
        const FString& StoredKey = Acks.StorageObjects[0].Key;
        NEXUS_SUCCESS("[NakamaMatchCode] Code '%s' stored in Nakama storage", *StoredKey);
        if (OnCodeStored) OnCodeStored(StoredKey);
    }
    else
    {
        NEXUS_WARN("[NakamaMatchCode] StoreCode succeeded but Acks array is empty");
        if (OnCodeStored) OnCodeStored(FString());
    }
}

void FNexusNakamaMatchCode::HandleStoreError(const FNakamaError& Error)
{
    NEXUS_ERROR("[NakamaMatchCode] StoreCode failed: %s", *Error.Message);
    if (OnCodeError) OnCodeError(Error.Message);
}

// ── LookupCode ────────────────────────────────────────────────────────────────

void FNexusNakamaMatchCode::LookupCode(const FString& Code)
{
    if (!EnsureClientAndSession(TEXT("LookupCode"))) return;

    if (Code.IsEmpty())
    {
        NEXUS_ERROR("[NakamaMatchCode] LookupCode: code is empty");
        if (OnCodeError) OnCodeError(TEXT("Match code is empty"));
        return;
    }

    PendingLookupCode = Code;

    NEXUS_LOG("[NakamaMatchCode] LookupCode: reading collection='%s' key='%s'",
        *Collection, *Code);

    // Read the storage object owned by whoever stored it.
    // PublicRead permission means we can read it even without knowing the owner.
    // We use empty string owner to let Nakama find it across all owners.
    FNakamaReadStorageObjectId ReadId;
    ReadId.Collection = Collection;
    ReadId.Key        = Code;
    ReadId.UserId     = LookupUserId;

    FOnStorageObjectsRead Success;
    Success.AddDynamic(Proxy, &UNexusNakamaMatchProxy::OnLookupSuccess);

    FOnError Error;
    Error.AddDynamic(Proxy, &UNexusNakamaMatchProxy::OnLookupError);

    Client->ReadStorageObjects(Session, {ReadId}, Success, Error);
}

void FNexusNakamaMatchCode::HandleLookupSuccess(
    const FNakamaStorageObjectList& Objects)
{
    if (Objects.Objects.Num() == 0)
    {
        NEXUS_WARN("[NakamaMatchCode] LookupCode: no object found for code '%s'",
            *PendingLookupCode);
        if (OnCodeError) OnCodeError(TEXT("Match code not found"));
        return;
    }

    const FString& Value = Objects.Objects[0].Value;

    NEXUS_LOG("[NakamaMatchCode] LookupCode: raw payload: %s", *Value);

    // Parse {"sessionId":"..."} manually — avoid pulling in a JSON lib
    FString SessionId;
    const FString SearchKey = TEXT("\"sessionId\":\"");
    int32 Start = Value.Find(SearchKey);
    if (Start != INDEX_NONE)
    {
        Start += SearchKey.Len();
        int32 End = Value.Find(TEXT("\""), ESearchCase::IgnoreCase,
            ESearchDir::FromStart, Start);
        if (End != INDEX_NONE)
            SessionId = Value.Mid(Start, End - Start);
    }

    if (SessionId.IsEmpty())
    {
        NEXUS_ERROR("[NakamaMatchCode] LookupCode: failed to parse sessionId from payload: %s",
            *Value);
        if (OnCodeError) OnCodeError(TEXT("Invalid match code payload"));
        return;
    }

    NEXUS_SUCCESS("[NakamaMatchCode] Code '%s' → SessionId '%s'",
        *PendingLookupCode, *SessionId);

    if (OnSessionFound) OnSessionFound(SessionId);
}

void FNexusNakamaMatchCode::HandleLookupError(const FNakamaError& Error)
{
    NEXUS_ERROR("[NakamaMatchCode] LookupCode failed for code '%s': %s",
        *PendingLookupCode, *Error.Message);
    if (OnCodeError) OnCodeError(Error.Message);
}

// ── DeleteCode ────────────────────────────────────────────────────────────────

void FNexusNakamaMatchCode::DeleteCode(const FString& Code)
{
    if (!EnsureClientAndSession(TEXT("DeleteCode"))) return;

    NEXUS_LOG("[NakamaMatchCode] DeleteCode: removing code '%s' from Nakama storage", *Code);

    FNakamaDeleteStorageObjectId DeleteId;
    DeleteId.Collection = Collection;
    DeleteId.Key        = Code;

    // SDK takes TFunction directly — no delegate type needed
    TFunction<void()> Success = [this, Code]()
    {
        NEXUS_SUCCESS("[NakamaMatchCode] Match code '%s' deleted from Nakama storage", *Code);
    };

    TFunction<void(const FNakamaError&)> Error = [this](const FNakamaError& Err)
    {
        // Non-fatal — leftover codes expire or get overwritten
        NEXUS_WARN("[NakamaMatchCode] DeleteCode failed (non-fatal): %s", *Err.Message);
    };

    Client->DeleteStorageObjects(Session, {DeleteId}, Success, Error);
}

void FNexusNakamaMatchCode::HandleDeleteSuccess()
{
    NEXUS_SUCCESS("[NakamaMatchCode] Match code deleted from Nakama storage");
}

void FNexusNakamaMatchCode::HandleDeleteError(const FNakamaError& Error)
{
    // Non-fatal — a leftover code just expires naturally or gets overwritten
    NEXUS_WARN("[NakamaMatchCode] DeleteCode failed (non-fatal): %s", *Error.Message);
}

// ── Helpers ───────────────────────────────────────────────────────────────────

bool FNexusNakamaMatchCode::EnsureClientAndSession(
    const FString& CallerName) const
{
    if (!Client)
    {
        NEXUS_ERROR("[NakamaMatchCode] %s: NakamaClient is null", *CallerName);
        if (OnCodeError) const_cast<FNexusNakamaMatchCode*>(this)
            ->OnCodeError(TEXT("Nakama client not initialized"));
        return false;
    }
    if (!Session || Session->IsExpired())
    {
        NEXUS_ERROR("[NakamaMatchCode] %s: Session is null or expired — re-login required",
            *CallerName);
        if (OnCodeError) const_cast<FNexusNakamaMatchCode*>(this)
            ->OnCodeError(TEXT("Nakama session expired"));
        return false;
    }
    return true;
}