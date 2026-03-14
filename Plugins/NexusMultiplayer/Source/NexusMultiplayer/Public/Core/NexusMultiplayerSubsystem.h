// Copyright NexusMultiplayer. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "NexusMultiplayerSettings.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Core/NexusTypes.h"
#include "OnlineSessionSettings.h"
#include "NexusMultiplayerSubsystem.generated.h"

class INexusOnlineInterface;
class INexusBackendInterface;
class INexusMatchCodeInterface;

UCLASS()
class NEXUSMULTIPLAYER_API UNexusMultiplayerSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    // ── USubsystem ────────────────────────────────────────────────────────────

    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
 
    virtual void Deinitialize() override;

    // ── Blueprint API — Session ───────────────────────────────────────────────

    UFUNCTION(BlueprintCallable, Category = "Nexus|Session",
        meta = (DisplayName = "Host Session"))
    void HostSession(FNexusSessionConfig Config);

    UFUNCTION(BlueprintCallable, Category = "Nexus|Session",
        meta = (DisplayName = "Find Sessions"))
    void FindSessions(int32 MaxResults = 100);

    UFUNCTION(BlueprintCallable, Category = "Nexus|Session",
        meta = (DisplayName = "Join By Code"))
    void JoinByCode(const FString& Code);

    UFUNCTION(BlueprintCallable, Category = "Nexus|Session",
        meta = (DisplayName = "Join Session From Row"))
    void JoinSessionFromRow(const FNexusServerRow& Row);

    UFUNCTION(BlueprintCallable, Category = "Nexus|Session",
        meta = (DisplayName = "Leave Session"))
    void LeaveSession();

    // ── Blueprint API — Queries ───────────────────────────────────────────────

    UFUNCTION(BlueprintPure, Category = "Nexus|Player")
    FString GetLocalDisplayName() const;

    UFUNCTION(BlueprintPure, Category = "Nexus|Session")
    FString GetActiveMatchCode() const { return ActiveMatchCode; }

    UFUNCTION(BlueprintPure, Category = "Nexus|Session")
    bool IsInSession() const;

    UFUNCTION(BlueprintPure, Category = "Nexus|Session")
    bool IsHost() const { return bIsHost; }

    UFUNCTION(BlueprintPure, Category = "Nexus|Backend")
    bool IsBackendConnected() const;

    UFUNCTION(BlueprintPure, Category = "Nexus|Session")
    bool IsSessionPrivate() const { return bIsPrivate; }

    // ── Delegates ─────────────────────────────────────────────────────────────

    UPROPERTY(BlueprintAssignable, Category = "Nexus|Events")
    FNexusOnSessionCreated  OnSessionCreated;

    UPROPERTY(BlueprintAssignable, Category = "Nexus|Events")
    FNexusOnSessionJoined   OnSessionJoined;

    UPROPERTY(BlueprintAssignable, Category = "Nexus|Events")
    FNexusOnSessionFailed   OnSessionFailed;

    UPROPERTY(BlueprintAssignable, Category = "Nexus|Events")
    FNexusOnSessionListReady OnSessionListReady;

    UPROPERTY(BlueprintAssignable, Category = "Nexus|Events")
    FNexusOnMatchCodeReady  OnMatchCodeReady;

    UPROPERTY(BlueprintAssignable, Category = "Nexus|Events")
    FNexusOnDisplayNameReady OnDisplayNameReady;

    // ── Internal access (C++ only) ────────────────────────────────────────────

    INexusOnlineInterface*    GetOnlineLayer()    const { return OnlineLayer.Get(); }
    INexusBackendInterface*   GetBackendLayer()   const { return BackendLayer.Get(); }
    INexusMatchCodeInterface* GetMatchCodeLayer() const { return MatchCodeLayer.Get(); }

    // Used by ANexusLobbyGameMode to get travel config
    UFUNCTION(BlueprintPure, Category = "Nexus|Session")
    FNexusSessionConfig GetLastSessionConfig() const { return LastSessionConfig; }

    UFUNCTION(BlueprintPure, Category = "Nexus|Identity")
    FString GetLocalUserId() const;

    UFUNCTION(BlueprintPure, Category = "Nexus|Session")
    bool IsJoinByCodeSupported() const
    {
        const UNexusMultiplayerSettings* S = UNexusMultiplayerSettings::Get();
        return S->BackendMode == ENexusBackendMode::Nakama
    || S->OnlineMode  == ENexusOnlineMode::Steam
    || S->OnlineMode  == ENexusOnlineMode::EOS;
    }

    UFUNCTION(BlueprintCallable, Category = "Nexus|Settings")
    void SetNetworkMode(ENexusNetworkMode NewMode);

    UFUNCTION(BlueprintPure, Category = "Nexus|Settings")
    ENexusNetworkMode GetNetworkMode() const { return NetworkMode; }

    void SetTravelingToGame(bool bValue) { bTravelingToGame = bValue; }

    // Leaderboard API
    UFUNCTION(BlueprintCallable, Category = "Nexus|Leaderboard")
    void SubmitScore(const FString& LeaderboardId, int64 Score);
 
    UFUNCTION(BlueprintCallable, Category = "Nexus|Leaderboard")
    void FetchTopScores(const FString& LeaderboardId, int32 Limit = 10);
 
    UFUNCTION(BlueprintPure, Category = "Nexus|Session")
    ENexusSessionState GetSessionState() const { return SessionState; }
 
    // Leaderboard delegates
    UPROPERTY(BlueprintAssignable, Category = "Nexus|Events")
    FNexusOnScoreSubmitted OnScoreSubmitted;
 
    UPROPERTY(BlueprintAssignable, Category = "Nexus|Events")
    FNexusOnTopScoresFetched OnTopScoresFetched;
 
    UPROPERTY(BlueprintAssignable, Category = "Nexus|Events")
    FNexusOnLeaderboardError OnLeaderboardError;
 
    UPROPERTY(BlueprintAssignable, Category = "Nexus|Events")
    FNexusOnSessionStateChanged OnSessionStateChanged;
    
private:
    // ── Layer instances ───────────────────────────────────────────────────────

    TSharedPtr<INexusOnlineInterface>    OnlineLayer;
    TSharedPtr<INexusBackendInterface>   BackendLayer;
    TSharedPtr<INexusMatchCodeInterface> MatchCodeLayer;

    // ── State ─────────────────────────────────────────────────────────────────

    FString             ActiveMatchCode;
    FNexusSessionConfig LastSessionConfig;
    bool                bIsHost    = false;
    bool                bIsPrivate = false;

    TArray<FOnlineSessionSearchResult> CachedSearchResults;

    // ── Init ──────────────────────────────────────────────────────────────────

    void SelectLayers();
    void BindLayerDelegates();

    // ── Online callbacks ──────────────────────────────────────────────────────

    void HandleCreateComplete(bool bSuccess);
    void HandleFindComplete(const TArray<FOnlineSessionSearchResult>& Results, bool bSuccess);
    void HandleJoinComplete(bool bSuccess);
    void HandleDestroyComplete(bool bSuccess);

    // ── Match code callbacks ──────────────────────────────────────────────────

    void HandleCodeStored(FString Code);
    void HandleSessionFound(FString SessionId);
    void HandleCodeError(FString Error);

    // ── Backend callbacks ─────────────────────────────────────────────────────

    void HandleAccountLoaded();
    void HandleDisplayNameUpdated(FString Name);
    void HandleLoginError(FString Error);

    // ── Helpers ───────────────────────────────────────────────────────────────

    UWorld* GetWorld() const;
    FNexusServerRow MakeServerRow(const FOnlineSessionSearchResult& Result) const;
    const FOnlineSessionSearchResult* FindResultBySessionId(const FString& SessionId) const;
    void TravelToSession();
    
    ENexusNetworkMode NetworkMode = ENexusNetworkMode::Internet;

    bool bTravelingToGame = false;

    ENexusSessionState SessionState = ENexusSessionState::Idle;
    void SetSessionState(ENexusSessionState NewState);
 
    void HandleScoreSubmitted(FString LeaderboardId);
    void HandleTopScoresFetched(FString LeaderboardId, TArray<FNexusLeaderboardEntry> Entries);
    void HandleLeaderboardError(FString Error);
    void ResolveDisplayName();

    FDelegateHandle EOSLoginHandle;
    void HandleEOSLoginComplete(int32 LocalUserNum, bool bWasSuccessful,
        const FUniqueNetId& UserId, const FString& Error);

    FTimerHandle DisplayNameRetryTimer;
    FString CachedDisplayName;

};