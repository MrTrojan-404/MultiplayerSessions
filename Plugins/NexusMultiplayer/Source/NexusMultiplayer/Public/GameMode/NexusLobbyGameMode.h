// Copyright NexusMultiplayer. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "NexusLobbyGameMode.generated.h"

struct FNexusPlayerInfo;
class ANexusLobbyGameState;
class UNexusMultiplayerSubsystem;

/**
 * ANexusLobbyGameMode
 *
 * Ready-to-use lobby game mode for the NexusMultiplayer plugin.
 * Set this (or a Blueprint subclass) as the GameMode on your lobby map.
 *
 * Behaviour:
 *   PostLogin  — adds player to ANexusLobbyGameState player list
 *              — calls TryTravelToGame when lobby is full
 *   Logout     — removes player from ANexusLobbyGameState player list
 *
 * Travel logic:
 *   Reads MaxPlayers and GameMapPath from the last FNexusSessionConfig
 *   passed to HostSession — no hardcoded map paths in the plugin.
 *
 * To customise travel (e.g. different maps per match type):
 *   Create a Blueprint subclass and override OnLobbyFull.
 */
UCLASS(Blueprintable)
class NEXUSMULTIPLAYER_API ANexusLobbyGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    ANexusLobbyGameMode();
    void BeginPlay();

    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual void Logout(AController* Exiting) override;

    // ── Blueprint override point ──────────────────────────────────────────────

    // Called when the lobby is full and it's time to travel to the game map.
    // Default implementation travels to FNexusSessionConfig.GameMapPath.
    // Override in Blueprint to add custom logic (e.g. per-match-type maps).
    UFUNCTION(BlueprintNativeEvent, Category = "Nexus|Lobby",
        meta = (DisplayName = "On Lobby Full"))
    void OnLobbyFull(const FString& GameMapPath, const FString& MatchType);
    virtual void OnLobbyFull_Implementation(
        const FString& GameMapPath, const FString& MatchType);

    // Called when a player joins the lobby — useful for sounds, UI triggers.
    UFUNCTION(BlueprintNativeEvent, Category = "Nexus|Lobby",
        meta = (DisplayName = "On Player Joined Lobby"))
    void OnPlayerJoinedLobby(const FNexusPlayerInfo& PlayerInfo);
    virtual void OnPlayerJoinedLobby_Implementation(
        const FNexusPlayerInfo& PlayerInfo) {}

    // Called when a player leaves the lobby.
    UFUNCTION(BlueprintNativeEvent, Category = "Nexus|Lobby",
        meta = (DisplayName = "On Player Left Lobby"))
    void OnPlayerLeftLobby(const FString& DisplayName);
    virtual void OnPlayerLeftLobby_Implementation(
        const FString& DisplayName) {}

private:
    void TryTravelToGame();
    void AddPlayerToGameState(APlayerController* PC);
    void RemovePlayerFromGameState(AController* Exiting);

    UNexusMultiplayerSubsystem* GetSubsystem() const;
    ANexusLobbyGameState*       GetNexusGameState() const;

    bool bMatchCodeBound = false;

    UFUNCTION()
    void HandleMatchCodeReady(const FString& Code);

    FString HostUserId;
};