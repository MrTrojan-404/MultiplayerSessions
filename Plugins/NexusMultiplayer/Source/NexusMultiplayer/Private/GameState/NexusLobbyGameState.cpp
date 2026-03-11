// Copyright NexusMultiplayer. All Rights Reserved.

#include "GameState/NexusLobbyGameState.h"
#include "Core/NexusLog.h"
#include "Net/UnrealNetwork.h"

ANexusLobbyGameState::ANexusLobbyGameState()
{
    bReplicates = true;
}

void ANexusLobbyGameState::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ANexusLobbyGameState, MatchCode);
    DOREPLIFETIME(ANexusLobbyGameState, PlayerList);
}

// ── Match Code ────────────────────────────────────────────────────────────────

void ANexusLobbyGameState::SetMatchCode(const FString& Code)
{
    if (!HasAuthority())
    {
        NEXUS_WARN("[LobbyGameState] SetMatchCode called on client — only host can set the code");
        return;
    }

    MatchCode = Code;

    NEXUS_SUCCESS("[LobbyGameState] Match code set: '%s' — replicating to clients", *Code);

    // Fire on host immediately (RepNotify only fires on clients)
    OnMatchCodeReady.Broadcast(MatchCode);
}

void ANexusLobbyGameState::OnRep_MatchCode()
{
    NEXUS_SUCCESS("[LobbyGameState] Match code replicated: '%s'", *MatchCode);
    OnMatchCodeReady.Broadcast(MatchCode);
}

// ── Player List ───────────────────────────────────────────────────────────────

void ANexusLobbyGameState::AddOrUpdatePlayer(const FNexusPlayerInfo& Info)
{
    if (!HasAuthority())
    {
        NEXUS_WARN("[LobbyGameState] AddOrUpdatePlayer called on client — only host can modify player list");
        return;
    }

    // Update existing entry if UserId matches
    for (FNexusPlayerInfo& Existing : PlayerList)
    {
        if (Existing.UserId == Info.UserId)
        {
            NEXUS_LOG("[LobbyGameState] Updating player: '%s' (UserId: %s)",
                *Info.DisplayName, *Info.UserId);
            Existing = Info;
            OnPlayerListUpdated.Broadcast(PlayerList);
            return;
        }
    }

    // New player
    NEXUS_LOG("[LobbyGameState] Adding player: '%s' (UserId: %s) — total: %d",
        *Info.DisplayName, *Info.UserId, PlayerList.Num() + 1);

    PlayerList.Add(Info);
    OnPlayerListUpdated.Broadcast(PlayerList);
}

void ANexusLobbyGameState::RemovePlayer(const FString& UserId)
{
    if (!HasAuthority())
    {
        NEXUS_WARN("[LobbyGameState] RemovePlayer called on client — only host can modify player list");
        return;
    }

    const int32 Before = PlayerList.Num();
    PlayerList.RemoveAll([&UserId](const FNexusPlayerInfo& P)
    {
        return P.UserId == UserId;
    });

    if (PlayerList.Num() < Before)
    {
        NEXUS_LOG("[LobbyGameState] Removed player UserId='%s' — remaining: %d",
            *UserId, PlayerList.Num());
        OnPlayerListUpdated.Broadcast(PlayerList);
    }
    else
    {
        NEXUS_WARN("[LobbyGameState] RemovePlayer: UserId='%s' not found in list", *UserId);
    }
}

void ANexusLobbyGameState::OnRep_PlayerList()
{
    NEXUS_LOG("[LobbyGameState] Player list replicated — count: %d", PlayerList.Num());
    OnPlayerListUpdated.Broadcast(PlayerList);
}