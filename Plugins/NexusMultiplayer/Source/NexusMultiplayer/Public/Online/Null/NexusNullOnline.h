// Copyright NexusMultiplayer. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Interfaces/INexusOnlineInterface.h"

/**
 * FNexusLANSession
 * Discovered session from custom UDP broadcast — no OSS dependency.
 */
struct FNexusLANSession
{
    FString SessionId;
    FString HostAddress;    // "192.168.x.x:7777" — used for ClientTravel
    FString MatchType;
    FString HostName;
    int32   MaxPlayers     = 0;
    int32   CurrentPlayers = 0;
};

/**
 * FNexusNullOnline
 *
 * LAN session implementation using custom UDP broadcast/listen.
 * No Steam, no EOS, no external service required.
 *
 * Host side:  CreateSession → broadcasts a UDP packet every 2s on port 7792
 * Client side: FindSessions → listens on port 7792 for 3s, collects results
 * Join:        ClientTravel directly to host IP (no OSS JoinSession needed)
 *
 * Suitable for:
 *   - Dev machines without Steam installed
 *   - Non-Steam marketplace buyers (EOS, itch.io, standalone)
 *   - Any platform where UE socket API is available
 */
class NEXUSMULTIPLAYER_API FNexusNullOnline : public INexusOnlineInterface
{
public:
    explicit FNexusNullOnline(UGameInstance* InGameInstance);
    virtual ~FNexusNullOnline() override;

    // ── INexusOnlineInterface ─────────────────────────────────────────────────

    virtual void    CreateSession(const FNexusSessionConfig& Config) override;
    virtual void    FindSessions(int32 MaxResults) override;
    virtual void    JoinSession(const FOnlineSessionSearchResult& Result) override;
    virtual void    JoinSessionById(const FString& SessionId) override;
    virtual void    FindSessionByAttribute(const FString& Key, const FString& Value) override;
    virtual void    DestroySession() override;
    virtual void    SetSessionAttribute(const FString& Key, const FString& Value) override;
    virtual FString GetCurrentSessionId() const override;
    
    virtual FString GetDisplayName() const override;
    virtual FString GetUserId()      const override;
    virtual FString GetAuthTicket()  const override;
    virtual bool    IsAvailable()    const override;

    virtual ENexusOnlineMode GetMode() const override { return ENexusOnlineMode::Null; }

    // ── Custom Discovery API (called by subsystem instead of OSS results) ─────

    // Returns true — subsystem skips OSS result conversion and calls GetDiscoveredRows()
    virtual bool UsesCustomDiscovery() const override { return true; }

    // Converts discovered LAN sessions to server rows for the UI
    virtual void GetDiscoveredRows(TArray<FNexusServerRow>& OutRows) const override;

private:
    TWeakObjectPtr<UGameInstance> GameInstance;
    UWorld* GetWorld() const;

    // ── UDP Broadcast (host) ──────────────────────────────────────────────────

    FSocket*     BroadcastSocket = nullptr;
    FTimerHandle BroadcastHandle;

    FString BroadcastSessionId;
    FString BroadcastMatchType;
    FString BroadcastHostName;
    int32   BroadcastMaxPlayers     = 0;
    int32   BroadcastCurrentPlayers = 1;

    void StartBroadcasting(const FNexusSessionConfig& Config);
    void StopBroadcasting();
    void SendBroadcastPacket();

    // ── UDP Listen (client) ───────────────────────────────────────────────────

    FSocket*     ListenSocket     = nullptr;
    FTimerHandle ListenHandle;
    FTimerHandle FindTimeoutHandle;

    TArray<FNexusLANSession> DiscoveredSessions;
    int32 FindMaxResults = 100;

    void StartListening(int32 MaxResults);
    void StopListening();
    void PollListenSocket();
    void OnFindTimeout();

    // ── Constants ─────────────────────────────────────────────────────────────

    static constexpr int32 BroadcastPort = 7792;
    static constexpr int32 GamePort      = 7777;
    static constexpr float BroadcastRate = 2.0f;   // seconds between packets
    static constexpr float FindDuration  = 3.0f;   // seconds to listen

    static const FString PacketPrefix;              // "NEXUS"
    static const FString PacketVersion;             // "1"

    // ── Packet helpers ────────────────────────────────────────────────────────

    FString BuildPacket() const;
    bool    ParsePacket(const FString& Raw, const FString& SenderIP,
                        FNexusLANSession& OutSession) const;
};