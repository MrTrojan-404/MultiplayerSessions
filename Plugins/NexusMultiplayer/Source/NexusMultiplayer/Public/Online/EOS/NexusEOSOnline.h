// Copyright NexusMultiplayer. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Interfaces/INexusOnlineInterface.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"

/**
 * FNexusEOSOnline
 *
 * EOS session layer — wraps OnlineSubsystemEOS via the standard
 * IOnlineSession interface. Mirrors FNexusSteamOnline almost exactly.
 *
 * Key differences from Steam:
 *   - OSS name is "EOS" instead of "STEAM"
 *   - IsAvailable() checks EOS identity login status
 *   - No SEARCH_LOBBIES filter (EOS uses presence-based search)
 *   - GetAuthTicket() returns the EOS connect token
 */
class NEXUSMULTIPLAYER_API FNexusEOSOnline : public INexusOnlineInterface
{
public:
    explicit FNexusEOSOnline(UGameInstance* InGameInstance);
    virtual ~FNexusEOSOnline() override;

    // INexusOnlineInterface
    virtual FString GetDisplayName() const override;
    virtual FString GetUserId()      const override;
    virtual FString GetAuthTicket()  const override;
    virtual bool    IsAvailable()    const override;
    virtual ENexusOnlineMode GetMode() const override { return ENexusOnlineMode::EOS; }

    virtual void CreateSession(const FNexusSessionConfig& Config) override;
    virtual void FindSessions(int32 MaxResults) override;
    virtual void FindSessionByAttribute(const FString& Key, const FString& Value) override;
    virtual void JoinSession(const FOnlineSessionSearchResult& Result) override;
    virtual void JoinSessionById(const FString& SessionId) override;
    virtual void DestroySession() override;
    virtual void SetSessionAttribute(const FString& Key, const FString& Value) override;

    // Helpers for NexusMultiplayerSubsystem
    virtual FString GetCurrentSessionId() const override;
    bool    GetSearchResultById(const FString& SessionId, FOnlineSessionSearchResult& OutResult) const;
    bool    IsSessionPrivate()      const { return bLastSessionWasPrivate; }
    int32   GetDesiredPlayerCount() const { return DesiredNumPublicConnections; }
    FString GetDesiredMatchType()   const { return DesiredMatchType; }

    UWorld* GetWorld() const
    {
        return GameInstance.IsValid() ? GameInstance->GetWorld() : nullptr;
    }

private:
    TWeakObjectPtr<UGameInstance>      GameInstance;
    IOnlineSessionPtr                  SessionInterface;
    TSharedPtr<FOnlineSessionSearch>   LastSessionSearch;
    TSharedPtr<FOnlineSessionSettings> LastSessionSettings;

    FDelegateHandle CreateSessionHandle;
    FDelegateHandle FindSessionsHandle;
    FDelegateHandle JoinSessionHandle;
    FDelegateHandle StartSessionHandle;
    FDelegateHandle DestroySessionHandle;

    FOnCreateSessionCompleteDelegate  OnCreateDelegate;
    FOnFindSessionsCompleteDelegate   OnFindDelegate;
    FOnJoinSessionCompleteDelegate    OnJoinDelegate;
    FOnStartSessionCompleteDelegate   OnStartDelegate;
    FOnDestroySessionCompleteDelegate OnDestroyDelegate;

    int32   DesiredNumPublicConnections = 4;
    FString DesiredMatchType;
    bool    bLastSessionWasPrivate      = false;

    bool    bCreateOnDestroy            = false;
    int32   PendingNumPublicConnections = 0;
    FString PendingMatchType;
    bool    bPendingPrivate             = false;

    bool    bFindingByAttribute         = false;
    FString PendingAttributeKey;
    FString PendingAttributeValue;

    void HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful);
    void HandleFindSessionsComplete(bool bWasSuccessful);
    void HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
    void HandleStartSessionComplete(FName SessionName, bool bWasSuccessful);
    void HandleDestroySessionComplete(FName SessionName, bool bWasSuccessful);

    bool EnsureSessionInterface();
    TSharedPtr<const FUniqueNetId> GetLocalPlayerId() const;
    void CreateSessionInternal(int32 NumPlayers, const FString& MatchType, bool bPrivate);
    void StartSession();
    IOnlineSubsystem* GetEOSOSS() const;
};