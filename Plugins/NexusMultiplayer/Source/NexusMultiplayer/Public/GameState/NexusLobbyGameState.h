// Copyright NexusMultiplayer. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Core/NexusTypes.h"
#include "NexusLobbyGameState.generated.h"

/**
 * ANexusLobbyGameState
 *
 * Replicated game state for the lobby map.
 * Host writes MatchCode and PlayerList — both replicate to all clients.
 *
 * Usage in your LobbyGameMode:
 *   GetGameState<ANexusLobbyGameState>()->SetMatchCode(Code);
 *   GetGameState<ANexusLobbyGameState>()->AddOrUpdatePlayer(Info);
 *
 * Set this as the GameStateClass in your LobbyGameMode defaults.
 */
UCLASS()
class NEXUSMULTIPLAYER_API ANexusLobbyGameState : public AGameStateBase
{
    GENERATED_BODY()

public:
    ANexusLobbyGameState();

    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // ── Match Code ────────────────────────────────────────────────────────────

    // Called by host (via LobbyGameMode or subsystem delegate)
    UFUNCTION(BlueprintCallable, Category = "Nexus|Lobby",
        meta = (DisplayName = "Set Match Code"))
    void SetMatchCode(const FString& Code);

    UFUNCTION(BlueprintPure, Category = "Nexus|Lobby")
    FString GetMatchCode() const { return MatchCode; }

    // Fires on clients when MatchCode replicates
    UPROPERTY(BlueprintAssignable, Category = "Nexus|Events")
    FNexusOnMatchCodeReady OnMatchCodeReady;

    // ── Player List ───────────────────────────────────────────────────────────

    // Host: add a new player or refresh an existing one (matched by UserId)
    UFUNCTION(BlueprintCallable, Category = "Nexus|Lobby",
        meta = (DisplayName = "Add Or Update Player"))
    void AddOrUpdatePlayer(const FNexusPlayerInfo& Info);

    // Host: remove a player by UserId (called from PostLogout)
    UFUNCTION(BlueprintCallable, Category = "Nexus|Lobby",
        meta = (DisplayName = "Remove Player"))
    void RemovePlayer(const FString& UserId);

    UFUNCTION(BlueprintPure, Category = "Nexus|Lobby")
    const TArray<FNexusPlayerInfo>& GetPlayerList() const { return PlayerList; }

    UFUNCTION(BlueprintPure, Category = "Nexus|Lobby")
    int32 GetPlayerCount() const { return PlayerList.Num(); }

    // Fires on all machines (host + clients) when PlayerList replicates
    UPROPERTY(BlueprintAssignable, Category = "Nexus|Events")
    FNexusOnPlayerListUpdated OnPlayerListUpdated;
    
private:
    // ── Replicated State ──────────────────────────────────────────────────────

    UPROPERTY(ReplicatedUsing = OnRep_MatchCode)
    FString MatchCode;

    UPROPERTY(ReplicatedUsing = OnRep_PlayerList)
    TArray<FNexusPlayerInfo> PlayerList;

    // ── RepNotifies ───────────────────────────────────────────────────────────

    UFUNCTION()
    void OnRep_MatchCode();

    UFUNCTION()
    void OnRep_PlayerList();
};