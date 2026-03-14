// Copyright NexusMultiplayer. All Rights Reserved.

#include "GameMode/NexusLobbyGameMode.h"
#include "GameState/NexusLobbyGameState.h"
#include "Core/NexusMultiplayerSubsystem.h"
#include "Core/NexusLog.h"
#include "Core/NexusTypes.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/World.h"
#include "Interfaces/INexusOnlineInterface.h"

ANexusLobbyGameMode::ANexusLobbyGameMode()
{
    // Set our replicated game state so MatchCode + PlayerList replicate to clients
    GameStateClass = ANexusLobbyGameState::StaticClass();

    bUseSeamlessTravel = true;
}

void ANexusLobbyGameMode::BeginPlay()
{
    Super::BeginPlay();

    if (UNexusMultiplayerSubsystem* Sub =
        GetGameInstance()->GetSubsystem<UNexusMultiplayerSubsystem>())
    {
        Sub->OnMatchCodeReady.AddDynamic(
            this, &ANexusLobbyGameMode::HandleMatchCodeReady);
        bMatchCodeBound = true;

        const FString Existing = Sub->GetActiveMatchCode();
        if (!Existing.IsEmpty())
            HandleMatchCodeReady(Existing);

        HostUserId = Sub->GetLocalUserId();
    }
}

// ── PostLogin ─────────────────────────────────────────────────────────────────

void ANexusLobbyGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);

    // Bind to subsystem OnMatchCodeReady if not already done
    if (!bMatchCodeBound)
    {
        UGameInstance* GI = GetGameInstance();
        if (UNexusMultiplayerSubsystem* Sub =
            GI->GetSubsystem<UNexusMultiplayerSubsystem>())
        {
            Sub->OnMatchCodeReady.AddDynamic(
                this, &ANexusLobbyGameMode::HandleMatchCodeReady);
            bMatchCodeBound = true;

            // Race condition fix — code may already be ready
            const FString Existing = Sub->GetActiveMatchCode();
            if (!Existing.IsEmpty())
                HandleMatchCodeReady(Existing);
        }
    }
    
    const int32 PlayerCount = GameState->PlayerArray.Num();

    NEXUS_LOG("[LobbyGameMode] PostLogin — total players: %d", PlayerCount);

    AddPlayerToGameState(NewPlayer);
    TryTravelToGame();
}


// ── Logout ────────────────────────────────────────────────────────────────────

void ANexusLobbyGameMode::Logout(AController* Exiting)
{
    RemovePlayerFromGameState(Exiting);

    NEXUS_LOG("[LobbyGameMode] Logout — remaining players: %d",
        GameState->PlayerArray.Num() - 1);

    Super::Logout(Exiting);
}

// ── Travel ────────────────────────────────────────────────────────────────────

void ANexusLobbyGameMode::TryTravelToGame()
{
    UNexusMultiplayerSubsystem* Sub = GetSubsystem();
    if (!Sub) return;

    const FNexusSessionConfig& Config = Sub->GetLastSessionConfig();
    const int32 CurrentPlayers = GameState->PlayerArray.Num();
    const int32 MaxPlayers     = Config.MaxPlayers;

    NEXUS_LOG("[LobbyGameMode] TryTravelToGame: %d / %d players",
        CurrentPlayers, MaxPlayers);

    if (CurrentPlayers < MaxPlayers)
    {
        NEXUS_LOG("[LobbyGameMode] Waiting for more players...");
        return;
    }

    if (Config.GameMapPath.IsEmpty())
    {
        NEXUS_ERROR("[LobbyGameMode] GameMapPath is empty — set it in FNexusSessionConfig "
                    "when calling HostSession");
        return;
    }

    NEXUS_SUCCESS("[LobbyGameMode] Lobby full (%d/%d) — firing OnLobbyFull",
        CurrentPlayers, MaxPlayers);

    OnLobbyFull(Config.GameMapPath, Config.MatchType);
}

void ANexusLobbyGameMode::OnLobbyFull_Implementation(
    const FString& GameMapPath, const FString& MatchType)
{
    NEXUS_SUCCESS("[LobbyGameMode] OnLobbyFull — traveling to: %s (MatchType: %s)",
        *GameMapPath, *MatchType);

    if (UNexusMultiplayerSubsystem* Sub = GetSubsystem())
    {
        if (INexusOnlineInterface* Online = Sub->GetOnlineLayer())
        {
            Online->SetSessionAttribute(TEXT("MapName"), GameMapPath);
            Online->SetSessionAttribute(TEXT("MatchType"), MatchType);
        }
    }

    bUseSeamlessTravel = true;
    GetWorld()->ServerTravel(GameMapPath);
}

// ── Player State Management ───────────────────────────────────────────────────

void ANexusLobbyGameMode::AddPlayerToGameState(APlayerController* PC)
{
    ANexusLobbyGameState* GS = GetNexusGameState();
    if (!GS || !PC) return;

    FNexusPlayerInfo Info;

    if (APlayerState* PS = PC->GetPlayerState<APlayerState>())
    {
        Info.DisplayName = PS->GetPlayerName();

        if (PS->GetUniqueId().IsValid())
            Info.UserId = PS->GetUniqueId()->ToString();
    }

    // Use backend display name if available — better than default platform name
    if (UNexusMultiplayerSubsystem* Sub = GetSubsystem())
    {
        if (Sub->IsBackendConnected())
        {
            const FString BackendName = Sub->GetLocalDisplayName();
            if (!BackendName.IsEmpty())
            {
                NEXUS_LOG("[LobbyGameMode] Using backend display name: '%s'", *BackendName);
                Info.DisplayName = BackendName;
            }
        }
    }

    // First player to join is the host
    Info.bIsHost = (Info.UserId == HostUserId);
    Info.bIsLocalPlayer = false; // server context has no local player

    NEXUS_LOG("[LobbyGameMode] Adding player: '%s' | UserId='%s' | IsHost=%s",
        *Info.DisplayName, *Info.UserId,
        Info.bIsHost ? TEXT("YES") : TEXT("NO"));

    GS->AddOrUpdatePlayer(Info);
    OnPlayerJoinedLobby(Info);
}

void ANexusLobbyGameMode::RemovePlayerFromGameState(AController* Exiting)
{
    ANexusLobbyGameState* GS = GetNexusGameState();
    if (!GS || !Exiting) return;

    APlayerState* PS = Exiting->GetPlayerState<APlayerState>();
    if (!PS) return;

    FString UserId;
    if (PS->GetUniqueId().IsValid())
        UserId = PS->GetUniqueId()->ToString();

    const FString DisplayName = PS->GetPlayerName();

    if (!UserId.IsEmpty())
    {
        NEXUS_LOG("[LobbyGameMode] Removing player: '%s'", *DisplayName);
        GS->RemovePlayer(UserId);
        OnPlayerLeftLobby(DisplayName);
    }
    else
    {
        NEXUS_WARN("[LobbyGameMode] Logout: no UserId for '%s' — cannot remove from list",
            *DisplayName);
    }
}

// ── Helpers ───────────────────────────────────────────────────────────────────

UNexusMultiplayerSubsystem* ANexusLobbyGameMode::GetSubsystem() const
{
    UGameInstance* GI = GetGameInstance();
    if (!GI)
    {
        NEXUS_ERROR("[LobbyGameMode] GetSubsystem: GameInstance is null");
        return nullptr;
    }
    return GI->GetSubsystem<UNexusMultiplayerSubsystem>();
}

ANexusLobbyGameState* ANexusLobbyGameMode::GetNexusGameState() const
{
    ANexusLobbyGameState* GS = GetGameState<ANexusLobbyGameState>();
    if (!GS)
    {
        NEXUS_ERROR("[LobbyGameMode] GetNexusGameState: GameState is not ANexusLobbyGameState — "
                    "make sure GameStateClass is set correctly in ANexusLobbyGameMode defaults");
    }
    return GS;
}
void ANexusLobbyGameMode::HandleMatchCodeReady(const FString& Code)
{
    NEXUS_SUCCESS("[LobbyGameMode] Match code received: '%s' — writing to GameState", *Code);

    if (ANexusLobbyGameState* GS = GetGameState<ANexusLobbyGameState>())
        GS->SetMatchCode(Code);
}