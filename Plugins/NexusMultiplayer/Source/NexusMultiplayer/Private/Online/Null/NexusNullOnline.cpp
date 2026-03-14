// Copyright NexusMultiplayer. All Rights Reserved.

#include "Online/Null/NexusNullOnline.h"
#include "Core/NexusLog.h"
#include "Core/NexusTypes.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"
#include "GameFramework/PlayerController.h"

// ── Statics ───────────────────────────────────────────────────────────────────

const FString FNexusNullOnline::PacketPrefix  = TEXT("NEXUS");
const FString FNexusNullOnline::PacketVersion = TEXT("1");

// Packet format (pipe-delimited, version-tagged for future compatibility):
// "NEXUS|1|SessionId|MatchType|MaxPlayers|CurrentPlayers|HostName|GamePort"

// ── Lifecycle ─────────────────────────────────────────────────────────────────

FNexusNullOnline::FNexusNullOnline(UGameInstance* InGameInstance)
    : GameInstance(InGameInstance)
{
    NEXUS_LOG("[NullOnline] Created — custom UDP LAN discovery (port %d)", BroadcastPort);
}

FNexusNullOnline::~FNexusNullOnline()
{
    StopBroadcasting();
    StopListening();
}

UWorld* FNexusNullOnline::GetWorld() const
{
    return GameInstance.IsValid() ? GameInstance->GetWorld() : nullptr;
}

// ── Identity ──────────────────────────────────────────────────────────────────

FString FNexusNullOnline::GetDisplayName() const
{
    const FString Name = FPlatformProcess::UserName(false);
    return Name.IsEmpty() ? TEXT("Player") : Name;
}

FString FNexusNullOnline::GetUserId() const
{
    return FPlatformMisc::GetLoginId();
}

FString FNexusNullOnline::GetAuthTicket() const
{
    return FString();
}

bool FNexusNullOnline::IsAvailable() const
{
    // Always available — no external service required
    return true;
}

// ── CreateSession ─────────────────────────────────────────────────────────────

void FNexusNullOnline::CreateSession(const FNexusSessionConfig& Config)
{
    NEXUS_LOG("[NullOnline] CreateSession: Players=%d | MatchType=%s",
        Config.MaxPlayers, *Config.MatchType);

    StopBroadcasting();
    StartBroadcasting(Config);

    // UE's listen server starts automatically when the subsystem calls
    // ServerTravel("Map?listen") after OnCreateComplete fires.
    // No OSS session registration needed for LAN.
    if (OnCreateComplete) OnCreateComplete(true);
}

// ── FindSessions ──────────────────────────────────────────────────────────────

void FNexusNullOnline::FindSessions(int32 MaxResults)
{
    NEXUS_LOG("[NullOnline] FindSessions: listening %.1fs on port %d",
        FindDuration, BroadcastPort);

    DiscoveredSessions.Empty();
    FindMaxResults = MaxResults;

    StopListening();
    StartListening(MaxResults);

    // Fire OnFindComplete after FindDuration even if nothing was found
    if (UWorld* W = GetWorld())
    {
        W->GetTimerManager().SetTimer(FindTimeoutHandle,
            [this]() { OnFindTimeout(); },
            FindDuration, false);
    }
    else
    {
        NEXUS_ERROR("[NullOnline] FindSessions: no world — cannot set timer");
        if (OnFindComplete) OnFindComplete({}, false);
    }
}

void FNexusNullOnline::OnFindTimeout()
{
    StopListening();

    NEXUS_LOG("[NullOnline] Find complete — discovered %d session(s)",
        DiscoveredSessions.Num());

    // Fire with empty OSS results — subsystem calls GetDiscoveredRows() instead
    // because UsesCustomDiscovery() returns true
    if (OnFindComplete) OnFindComplete({}, true);
}

// ── JoinSession ───────────────────────────────────────────────────────────────

void FNexusNullOnline::JoinSession(const FOnlineSessionSearchResult& Result)
{
    // LAN joins are handled via ClientTravel in JoinSessionFromRow.
    // This path is unreachable when UsesCustomDiscovery() is true.
    NEXUS_WARN("[NullOnline] JoinSession(OSS) called on LAN — use JoinSessionFromRow");
    if (OnJoinComplete) OnJoinComplete(false);
}

void FNexusNullOnline::JoinSessionById(const FString& SessionId)
{
    NEXUS_WARN("[NullOnline] JoinSessionById not supported on LAN — use server browser");
    if (OnJoinComplete) OnJoinComplete(false);
}

void FNexusNullOnline::FindSessionByAttribute(const FString& Key, const FString& Value)
{
    NEXUS_WARN("[NullOnline] FindSessionByAttribute not supported on LAN");
    if (OnAttributeSessionFound) OnAttributeSessionFound(FString());
}

// ── DestroySession ────────────────────────────────────────────────────────────

void FNexusNullOnline::DestroySession()
{
    NEXUS_LOG("[NullOnline] DestroySession — stopping broadcast");
    StopBroadcasting();
    StopListening();
    if (OnDestroyComplete) OnDestroyComplete(true);
}

// ── SetSessionAttribute ───────────────────────────────────────────────────────

void FNexusNullOnline::SetSessionAttribute(const FString& Key, const FString& Value)
{
    // LAN sessions don't support runtime attribute updates
    NEXUS_WARN("[NullOnline] SetSessionAttribute no-op on LAN (Key='%s')", *Key);
}

// Won't be using this anywhere, so return null
FString FNexusNullOnline::GetCurrentSessionId() const
{
   return FString();
}

// ── Custom Discovery ──────────────────────────────────────────────────────────

void FNexusNullOnline::GetDiscoveredRows(TArray<FNexusServerRow>& OutRows) const
{
    OutRows.Reserve(DiscoveredSessions.Num());

    for (const FNexusLANSession& S : DiscoveredSessions)
    {
        FNexusServerRow Row;
        Row.SessionId      = S.SessionId;
        Row.HostName       = S.HostName;
        Row.MatchType      = S.MatchType;
        Row.MaxPlayers     = S.MaxPlayers;
        Row.CurrentPlayers = S.CurrentPlayers;
        Row.PingMs         = 0;            // no ping measurement for LAN
        Row.HostAddress    = S.HostAddress; // "IP:Port" — used for ClientTravel
        OutRows.Add(Row);
    }
}

// ── UDP Broadcast (host side) ─────────────────────────────────────────────────

void FNexusNullOnline::StartBroadcasting(const FNexusSessionConfig& Config)
{
    ISocketSubsystem* SS = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (!SS)
    {
        NEXUS_ERROR("[NullOnline] StartBroadcasting: no socket subsystem");
        return;
    }

    BroadcastSocket = SS->CreateSocket(NAME_DGram, TEXT("NexusBroadcast"), false);
    if (!BroadcastSocket)
    {
        NEXUS_ERROR("[NullOnline] StartBroadcasting: socket create failed");
        return;
    }

    BroadcastSocket->SetBroadcast(true);
    BroadcastSocket->SetNonBlocking(true);

    // Cache session info for packet building
    BroadcastSessionId      = FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(12);
    BroadcastMatchType      = Config.MatchType;
    BroadcastMaxPlayers     = Config.MaxPlayers;
    BroadcastCurrentPlayers = 1;
    BroadcastHostName       = GetDisplayName();

    NEXUS_LOG("[NullOnline] Broadcasting session '%s' every %.1fs on port %d",
        *BroadcastSessionId, BroadcastRate, BroadcastPort);

    // Send immediately so clients don't wait for the first interval
    SendBroadcastPacket();

    if (UWorld* W = GetWorld())
    {
        W->GetTimerManager().SetTimer(BroadcastHandle,
            [this]() { SendBroadcastPacket(); },
            BroadcastRate, true);
    }
}

void FNexusNullOnline::SendBroadcastPacket()
{
    if (!BroadcastSocket) return;

    ISocketSubsystem* SS = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (!SS) return;

    TSharedRef<FInternetAddr> Addr = SS->CreateInternetAddr();
    Addr->SetBroadcastAddress();
    Addr->SetPort(BroadcastPort);

    const FString    Packet = BuildPacket();
    const FTCHARToUTF8 UTF8(*Packet);

    int32 BytesSent = 0;
    if (!BroadcastSocket->SendTo(
            (const uint8*)UTF8.Get(), UTF8.Length(), BytesSent, *Addr))
    {
        NEXUS_WARN("[NullOnline] SendBroadcastPacket: send failed");
    }
}

void FNexusNullOnline::StopBroadcasting()
{
    if (UWorld* W = GetWorld())
        W->GetTimerManager().ClearTimer(BroadcastHandle);

    if (BroadcastSocket)
    {
        BroadcastSocket->Close();
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)
            ->DestroySocket(BroadcastSocket);
        BroadcastSocket = nullptr;
        NEXUS_LOG("[NullOnline] Broadcast stopped");
    }
}

// ── UDP Listen (client side) ──────────────────────────────────────────────────

void FNexusNullOnline::StartListening(int32 MaxResults)
{
    ISocketSubsystem* SS = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (!SS)
    {
        NEXUS_ERROR("[NullOnline] StartListening: no socket subsystem");
        return;
    }

    ListenSocket = SS->CreateSocket(NAME_DGram, TEXT("NexusListen"), true);
    if (!ListenSocket)
    {
        NEXUS_ERROR("[NullOnline] StartListening: socket create failed");
        return;
    }

    ListenSocket->SetReuseAddr(true);
    ListenSocket->SetNonBlocking(true);

    TSharedRef<FInternetAddr> BindAddr = SS->CreateInternetAddr();
    BindAddr->SetAnyAddress();
    BindAddr->SetPort(BroadcastPort);

    if (!ListenSocket->Bind(*BindAddr))
    {
        NEXUS_ERROR("[NullOnline] StartListening: bind failed on port %d "
            "(port in use? firewall?)", BroadcastPort);
        ListenSocket->Close();
        SS->DestroySocket(ListenSocket);
        ListenSocket = nullptr;
        return;
    }

    NEXUS_LOG("[NullOnline] Listening on port %d (%.1fs timeout)", BroadcastPort, FindDuration);

    if (UWorld* W = GetWorld())
    {
        W->GetTimerManager().SetTimer(ListenHandle,
            [this]() { PollListenSocket(); },
            0.25f, true);
    }
}

void FNexusNullOnline::PollListenSocket()
{
    if (!ListenSocket) return;

    ISocketSubsystem* SS = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (!SS) return;

    uint8 Buffer[512];
    int32 BytesRead = 0;
    TSharedRef<FInternetAddr> Sender = SS->CreateInternetAddr();

    while (ListenSocket->RecvFrom(Buffer, sizeof(Buffer) - 1, BytesRead, *Sender))
    {
        if (BytesRead <= 0) break;
        Buffer[BytesRead] = '\0';

        const FString Raw       = UTF8_TO_TCHAR((const ANSICHAR*)Buffer);
        const FString SenderIP  = Sender->ToString(false); // IP only, no port

        FNexusLANSession Session;
        if (!ParsePacket(Raw, SenderIP, Session)) continue;

        // Deduplicate by SessionId — update if already seen
        bool bExists = false;
        for (FNexusLANSession& Existing : DiscoveredSessions)
        {
            if (Existing.SessionId == Session.SessionId)
            {
                Existing = Session;
                bExists  = true;
                break;
            }
        }

        if (!bExists)
        {
            NEXUS_LOG("[NullOnline] Discovered: '%s' @ %s | %s | %d/%d players",
                *Session.SessionId, *Session.HostAddress,
                *Session.MatchType, Session.CurrentPlayers, Session.MaxPlayers);

            DiscoveredSessions.Add(Session);

            // Stop early if we hit the max
            if (DiscoveredSessions.Num() >= FindMaxResults)
            {
                OnFindTimeout();
                return;
            }
        }
    }
}

void FNexusNullOnline::StopListening()
{
    if (UWorld* W = GetWorld())
    {
        W->GetTimerManager().ClearTimer(ListenHandle);
        W->GetTimerManager().ClearTimer(FindTimeoutHandle);
    }

    if (ListenSocket)
    {
        ListenSocket->Close();
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)
            ->DestroySocket(ListenSocket);
        ListenSocket = nullptr;
    }
}

// ── Packet codec ──────────────────────────────────────────────────────────────
// Format: "NEXUS|1|SessionId|MatchType|MaxPlayers|CurrentPlayers|HostName|GamePort"

FString FNexusNullOnline::BuildPacket() const
{
    // Sanitise HostName — strip pipe chars to avoid parse corruption
    const FString SafeName = BroadcastHostName.Replace(TEXT("|"), TEXT("_"));

    return FString::Printf(TEXT("%s|%s|%s|%s|%d|%d|%s|%d"),
        *PacketPrefix,
        *PacketVersion,
        *BroadcastSessionId,
        *BroadcastMatchType,
        BroadcastMaxPlayers,
        BroadcastCurrentPlayers,
        *SafeName,
        GamePort);
}

bool FNexusNullOnline::ParsePacket(
    const FString& Raw, const FString& SenderIP, FNexusLANSession& Out) const
{
    TArray<FString> Parts;
    Raw.ParseIntoArray(Parts, TEXT("|"));

    // Minimum 8 fields required
    if (Parts.Num() < 8)            return false;
    if (Parts[0] != PacketPrefix)   return false;
    if (Parts[1] != PacketVersion)  return false; // reject future versions

    Out.SessionId      = Parts[2];
    Out.MatchType      = Parts[3];
    Out.MaxPlayers     = FCString::Atoi(*Parts[4]);
    Out.CurrentPlayers = FCString::Atoi(*Parts[5]);
    Out.HostName       = Parts[6];
    const int32 Port   = FCString::Atoi(*Parts[7]);

    Out.HostAddress    = FString::Printf(TEXT("%s:%d"), *SenderIP, Port);

    // Sanity check
    if (Out.SessionId.IsEmpty() || Out.MaxPlayers <= 0 || Port <= 0)
        return false;

    return true;
}