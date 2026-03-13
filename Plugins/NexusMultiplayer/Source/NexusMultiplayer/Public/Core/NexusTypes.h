// Copyright NexusMultiplayer. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "OnlineSessionSettings.h"
#include "NexusTypes.generated.h"

// ─── Enums ────────────────────────────────────────────────────────────────────

UENUM(BlueprintType)
enum class ENexusOnlineMode : uint8
{
    Steam   UMETA(DisplayName = "Steam"),
    EOS     UMETA(DisplayName = "EOS (Epic Online Services)"),
    Null    UMETA(DisplayName = "Null (LAN / Dev)"),
};

UENUM(BlueprintType)
enum class ENexusBackendMode : uint8
{
    PlatformOnly    UMETA(DisplayName = "Platform Only (No Server)"),
    Nakama          UMETA(DisplayName = "Nakama"),
};

UENUM(BlueprintType)
enum class ENexusSessionState : uint8
{
    // Not in a session, not trying to join one
    Idle            UMETA(DisplayName = "Idle"),

    // CreateSession call is in-flight
    Creating        UMETA(DisplayName = "Creating"),

    // Session exists and is open — host is waiting for players
    Hosting         UMETA(DisplayName = "Hosting"),

    // FindSessions call is in-flight
    Searching       UMETA(DisplayName = "Searching"),

    // JoinSession call is in-flight
    Joining         UMETA(DisplayName = "Joining"),

    // Fully joined and playing
    InSession       UMETA(DisplayName = "In Session"),

    // DestroySession call is in-flight
    Leaving         UMETA(DisplayName = "Leaving"),
};

// ─── Session Config ───────────────────────────────────────────────────────────

// Passed to HostSession from Blueprint or C++
USTRUCT(BlueprintType)
struct FNexusSessionConfig
{
    GENERATED_BODY()

    // Max number of players including host
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nexus|Session")
    int32 MaxPlayers = 4;

    // Full path to the lobby map e.g. "/Game/Maps/Lobby"
    // Plugin appends "?listen" automatically
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nexus|Session")
    FString LobbyMapPath = TEXT("/Game/Maps/Lobby");

    // Not EditAnywhere — set at runtime only, not shown in Details panel
    UPROPERTY(BlueprintReadWrite, Category = "Nexus|Session")
    FString GameMapPath = TEXT("");

    // Tag used to filter sessions in the server browser
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nexus|Session")
    FString MatchType = TEXT("FreeForAll");

    // If true, session is not advertised publicly — join by code only
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nexus|Session")
    bool bPrivate = false;
};

// ─── Server Browser Row ───────────────────────────────────────────────────────

// UI-friendly representation of a session search result.
// Bind to OnSessionListReady to populate your server browser widget.
USTRUCT(BlueprintType)
struct FNexusServerRow
{
    GENERATED_BODY()

    // Internal session ID — passed to JoinSessionFromRow
    UPROPERTY(BlueprintReadOnly, Category = "Nexus|ServerRow")
    FString SessionId;

    // Steam / EOS display name of the session host
    UPROPERTY(BlueprintReadOnly, Category = "Nexus|ServerRow")
    FString HostName;

    // Number of players currently in the session
    UPROPERTY(BlueprintReadOnly, Category = "Nexus|ServerRow")
    int32 CurrentPlayers = 0;

    // Session capacity
    UPROPERTY(BlueprintReadOnly, Category = "Nexus|ServerRow")
    int32 MaxPlayers = 0;

    // Estimated ping to the host in milliseconds
    UPROPERTY(BlueprintReadOnly, Category = "Nexus|ServerRow")
    int32 PingMs = 0;

    // MatchType tag set by the host at session creation
    UPROPERTY(BlueprintReadOnly, Category = "Nexus|ServerRow")
    FString MatchType;

    // Map the session is running on
    UPROPERTY(BlueprintReadOnly, Category = "Nexus|ServerRow")
    FString MapName;

    // LAN only — "IP:Port" for ClientTravel direct join. Empty for Steam/OSS sessions.
    UPROPERTY(BlueprintReadOnly) FString HostAddress;

    // Not exposed to Blueprint — OSS search result for Steam joins
    FOnlineSessionSearchResult SearchResult;
    
    // Helper — "2 / 4"
    FString GetPlayerCountString() const
    {
        return FString::Printf(TEXT("%d / %d"), CurrentPlayers, MaxPlayers);
    }

    // Helper — "42 ms"
    FString GetPingString() const
    {
        return FString::Printf(TEXT("%d ms"), PingMs);
    }
};

// ─── Player Info ──────────────────────────────────────────────────────────────

// Represents one player in the lobby list.
// Populated by the host's LobbyGameState and replicated to all clients.
USTRUCT(BlueprintType)
struct FNexusPlayerInfo
{
    GENERATED_BODY()

    // Platform user ID (SteamID64 string, EOS ProductUserId, etc.)
    UPROPERTY(BlueprintReadOnly, Category = "Nexus|Player")
    FString UserId;

    // Resolved display name — Steam nickname, Nakama username, or OS fallback
    UPROPERTY(BlueprintReadOnly, Category = "Nexus|Player")
    FString DisplayName;

    // True if this entry represents the local player
    UPROPERTY(BlueprintReadOnly, Category = "Nexus|Player")
    bool bIsLocalPlayer = false;

    // True if this player is the session host
    UPROPERTY(BlueprintReadOnly, Category = "Nexus|Player")
    bool bIsHost = false;

    // Ping in ms — updated periodically by the host
    UPROPERTY(BlueprintReadOnly, Category = "Nexus|Player")
    int32 PingMs = 0;
};


// ─── Leaderboard Entry ────────────────────────────────────────────────────────
 
USTRUCT(BlueprintType)
struct FNexusLeaderboardEntry
{
    GENERATED_BODY()
 
    // Nakama user ID
    UPROPERTY(BlueprintReadOnly, Category = "Nexus|Leaderboard")
    FString UserId;
 
    // Display name resolved by Nakama
    UPROPERTY(BlueprintReadOnly, Category = "Nexus|Leaderboard")
    FString Username;
 
    // The submitted score
    UPROPERTY(BlueprintReadOnly, Category = "Nexus|Leaderboard")
    int64 Score = 0;
 
    // Secondary sort score (optional — e.g. time to achieve the score)
    UPROPERTY(BlueprintReadOnly, Category = "Nexus|Leaderboard")
    int64 SubScore = 0;
 
    // Rank on the leaderboard (1 = first)
    UPROPERTY(BlueprintReadOnly, Category = "Nexus|Leaderboard")
    int32 Rank = 0;
};

USTRUCT(BlueprintType)
struct FNexusMapEntry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString DisplayName = TEXT("");

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString LobbyMapPath = TEXT("");

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString GameMapPath = TEXT("");

    // Optional: thumbnail shown in map picker
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TObjectPtr<UTexture2D> Thumbnail = nullptr;
};

UENUM(BlueprintType)
enum class ENexusNetworkMode : uint8
{
    Internet UMETA(DisplayName = "Internet"),
    LAN      UMETA(DisplayName = "LAN")
};

// ─── Delegates ────────────────────────────────────────────────────────────────
// Declared here so subsystem, widgets, and game code can bind without
// circular includes. Always bind via the subsystem's UPROPERTY delegates.

// A private session was hosted — Code is the 6-char join code to display
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FNexusOnMatchCodeReady,
    const FString&, Code);

// Backend resolved the local player's display name
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FNexusOnDisplayNameReady,
    const FString&, Name);

// Any session operation failed — Error is a human-readable message for UI
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FNexusOnSessionFailed,
    const FString&, Error);

// FindSessions completed — Rows is the filtered, UI-ready result list
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FNexusOnSessionListReady,
    const TArray<FNexusServerRow>&, Rows);

// HostSession completed
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FNexusOnSessionCreated,
    bool, bWasSuccessful);

// JoinSession or JoinByCode completed
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FNexusOnSessionJoined,
    bool, bWasSuccessful);

// Subsystem state changed — useful for driving loading spinners in UI
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FNexusOnSessionStateChanged,
    ENexusSessionState, OldState,
    ENexusSessionState, NewState);

// Lobby player list changed — bind in your lobby widget to refresh the list
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FNexusOnPlayerListUpdated,
    const TArray<FNexusPlayerInfo>&, Players);

// Score submitted successfully
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FNexusOnScoreSubmitted,
    const FString&, LeaderboardId);
 
// Top scores fetched — Entries is ordered by rank
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FNexusOnTopScoresFetched,
    const FString&, LeaderboardId,
    const TArray<FNexusLeaderboardEntry>&, Entries);
 
// Leaderboard operation failed
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FNexusOnLeaderboardError,
    const FString&, Error);
 