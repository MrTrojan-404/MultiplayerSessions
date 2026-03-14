// Copyright NexusMultiplayer. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Interfaces/INexusOnlineInterface.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"

class NEXUSMULTIPLAYER_API FNexusSteamOnline : public INexusOnlineInterface
{
public:
    explicit FNexusSteamOnline(UGameInstance* InGameInstance);
    virtual ~FNexusSteamOnline() override;

    // INexusOnlineInterface
    virtual FString GetDisplayName() const override;
    virtual FString GetUserId() const override;
    virtual FString GetAuthTicket() const override;
    virtual bool IsAvailable() const override;
    virtual ENexusOnlineMode GetMode() const override { return ENexusOnlineMode::Steam; }
    virtual void CreateSession(const FNexusSessionConfig& Config) override;
    virtual void FindSessions(int32 MaxResults) override;
    virtual void FindSessionByAttribute(const FString& Key, const FString& Value) override;
    virtual void JoinSession(const FOnlineSessionSearchResult& Result) override;
    virtual void JoinSessionById(const FString& SessionId) override;
    virtual void DestroySession() override;
    virtual void SetSessionAttribute(const FString& Key, const FString& Value) override;
    
    // Helpers for NexusMultiplayerSubsystem
    virtual FString GetCurrentSessionId() const override ;
    bool    GetSearchResultById(const FString& SessionId, FOnlineSessionSearchResult& OutResult) const;
    bool    IsSessionPrivate() const { return bLastSessionWasPrivate; }
    int32   GetDesiredPlayerCount() const { return DesiredNumPublicConnections; }
    FString GetDesiredMatchType() const { return DesiredMatchType; }

    // Helper — gets world lazily at call time, never stale
    UWorld* GetWorld() const 
    {
	    return GameInstance.IsValid() ? GameInstance->GetWorld() : nullptr;
    }

    
private:
    TWeakObjectPtr<UGameInstance> GameInstance;
    IOnlineSessionPtr SessionInterface;
    TSharedPtr<FOnlineSessionSearch> LastSessionSearch;
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

    bool    bFindingBySessionId         = false;
    FString PendingFindSessionId;

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
};