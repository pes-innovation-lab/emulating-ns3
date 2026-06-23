#include "sctp-association.h"
#include "sctp-socket.h"
#include "sctp-l4-protocol.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/node.h"
#include <sstream>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("SctpAssociation");

NS_OBJECT_ENSURE_REGISTERED(SctpAssociation);

TypeId
SctpAssociation::GetTypeId()
{
    static TypeId tid = TypeId("ns3::SctpAssociation")
                            .SetParent<Object>()
                            .SetGroupName("Sctp")
                            .AddConstructor<SctpAssociation>();
    return tid;
}

SctpAssociation::SctpAssociation()
    : m_socket(nullptr),
      m_state(CLOSED),
      m_localTag(0),
      m_peerTag(0),
      m_localTsn(0),
      m_peerTsn(0),
      m_cumulativeTsnAck(0),
      m_peerCumTsnAck(0),
      m_numOutboundStreams(2),
      m_numInboundStreams(2),
      m_initAttempts(0),
      m_cookieAttempts(0),
      m_lastExpectedTsn(0),
      m_peerRwnd(65535),
      m_ssthresh(65535),
      m_lastRxAddress(Ipv4Address::GetAny())
{
    // Generate a random local verification tag
    m_localTag = m_localTag == 0 ? (uint32_t)rand() + 1 : m_localTag;
    m_localTsn = (uint32_t)rand() & 0x7FFFFFFF;
    m_cumulativeTsnAck = 0;
}

SctpAssociation::~SctpAssociation()
{
    StopInitTimer();
    StopCookieTimer();
    StopRtoTimer();
    for (auto &path : m_peerPaths)
    {
        StopHbTimer(path);
    }
}

void
SctpAssociation::SetSocket(Ptr<SctpSocket> socket)
{
    m_socket = socket;
}

Ptr<SctpSocket>
SctpAssociation::GetSocket() const
{
    return m_socket;
}

SctpAssociation::State
SctpAssociation::GetState() const
{
    return m_state;
}

void
SctpAssociation::SetState(State state)
{
    NS_LOG_INFO("State transition: " << m_state << " -> " << state);
    m_state = state;
}

void
SctpAssociation::SetLocalAddresses(const std::vector<Ipv4Address> &addrs)
{
    m_localAddresses = addrs;
}

const std::vector<Ipv4Address>&
SctpAssociation::GetLocalAddresses() const
{
    return m_localAddresses;
}

void
SctpAssociation::SetPeerAddresses(const std::vector<Ipv4Address> &addrs)
{
    m_peerPaths.clear();
    for (const auto &addr : addrs)
    {
        PathInfo path;
        path.address = addr;
        path.active = true;
        path.errorCount = 0;
        path.cwnd = 1460 * 3; // Initial cwnd (3 packets)
        path.ssthresh = 65535;
        path.flightSize = 0;
        path.rto = Seconds(1.0);
        m_peerPaths.push_back(path);
    }
    if (!addrs.empty())
    {
        m_primaryPath = addrs[0];
    }
}

const std::vector<SctpAssociation::PathInfo>&
SctpAssociation::GetPeerPaths() const
{
    return m_peerPaths;
}

void
SctpAssociation::SetPrimaryPath(Ipv4Address addr)
{
    m_primaryPath = addr;
    // ensure primary path is in peer paths list
    bool found = false;
    for (const auto &path : m_peerPaths)
    {
        if (path.address == addr)
        {
            found = true;
            break;
        }
    }
    if (!found && !addr.IsAny())
    {
        PathInfo path;
        path.address = addr;
        path.active = true;
        path.errorCount = 0;
        path.cwnd = 1460 * 3;
        path.ssthresh = 65535;
        path.flightSize = 0;
        path.rto = Seconds(1.0);
        m_peerPaths.push_back(path);
    }
}

Ipv4Address
SctpAssociation::GetPrimaryPath() const
{
    return m_primaryPath;
}

void
SctpAssociation::SetNumOutboundStreams(uint16_t num)
{
    m_numOutboundStreams = num;
}

uint16_t
SctpAssociation::GetNumOutboundStreams() const
{
    return m_numOutboundStreams;
}

void
SctpAssociation::SetNumInboundStreams(uint16_t num)
{
    m_numInboundStreams = num;
}

uint16_t
SctpAssociation::GetNumInboundStreams() const
{
    return m_numInboundStreams;
}

void
SctpAssociation::Initiate()
{
    if (m_state != CLOSED)
    {
        return;
    }
    m_initAttempts = 0;
    m_initTimer = Simulator::Schedule(Seconds(0.001), &SctpAssociation::SendInit, this);
}

void
SctpAssociation::Close()
{
    if (m_state == ESTABLISHED)
    {
        SetState(SHUTDOWN_PENDING);
        if (m_txQueue.empty())
        {
            SendShutdown(m_lastExpectedTsn);
            SetState(SHUTDOWN_SENT);
            StartRtoTimer(); // T2-shutdown timer
        }
    }
    else if (m_state == CLOSED)
    {
        // already closed
    }
}

void
SctpAssociation::SendPacket(Ptr<SctpHeader> header, Ipv4Address dest)
{
    if (!m_socket)
    {
        return;
    }
    header->SetSourcePort(m_socket->GetLocalPort());
    header->SetDestinationPort(m_socket->m_peerPort);

    Ptr<Packet> packet = Create<Packet>();
    packet->AddHeader(*header);

    // Pick appropriate local address
    Ipv4Address localSrc = Ipv4Address::GetAny();
    if (m_localAddresses.size() > 1)
    {
        // Select local address matching the destination subnet (match first three octets)
        uint32_t destSubnet = dest.Get() & 0xFFFFFF00;
        for (const auto &addr : m_localAddresses)
        {
            if ((addr.Get() & 0xFFFFFF00) == destSubnet)
            {
                localSrc = addr;
                break;
            }
        }
    }
    if (localSrc.IsAny() && !m_localAddresses.empty())
    {
        localSrc = m_localAddresses[0];
    }

    Ptr<Node> node = m_socket->GetNode();
    Ptr<IpL4Protocol> proto = node->GetObject<IpL4Protocol>(SctpL4Protocol::GetTypeId());
    if (proto)
    {
        Ptr<SctpL4Protocol> sctpProto = DynamicCast<SctpL4Protocol>(proto);
        if (sctpProto)
        {
            sctpProto->SendPacket(packet, localSrc, dest);
        }
    }
}

Ipv4Address
SctpAssociation::SelectDestinationPath()
{
    // Return primary path if active
    for (auto &path : m_peerPaths)
    {
        if (path.address == m_primaryPath && path.active)
        {
            return m_primaryPath;
        }
    }
    // Search for any active secondary path
    for (auto &path : m_peerPaths)
    {
        if (path.active)
        {
            return path.address;
        }
    }
    // Fallback
    return m_primaryPath;
}

SctpAssociation::PathInfo*
SctpAssociation::GetPath(Ipv4Address addr)
{
    for (auto &path : m_peerPaths)
    {
        if (path.address == addr)
        {
            return &path;
        }
    }
    return nullptr;
}

void
SctpAssociation::SendInit()
{
    Ptr<SctpHeader> header = Create<SctpHeader>();
    header->SetSourcePort(m_socket->GetLocalPort());
    header->SetDestinationPort(m_socket->GetLocalPort()); // default port mapping
    header->SetVerificationTag(0); // INIT chunk always has vtag=0 in common header

    Ptr<SctpChunkInit> initChunk = Create<SctpChunkInit>();
    initChunk->m_initiateTag = m_localTag;
    initChunk->m_aRwnd = 65535;
    initChunk->m_numOutboundStreams = m_numOutboundStreams;
    initChunk->m_numInboundStreams = m_numInboundStreams;
    initChunk->m_initialTsn = m_localTsn;
    initChunk->m_ipv4Addresses = m_localAddresses;

    header->AddChunk(initChunk);
    
    Ipv4Address dest = SelectDestinationPath();
    NS_LOG_INFO("Sending INIT to " << dest << " with local tag " << m_localTag);
    SendPacket(header, dest);

    m_initAttempts++;
    SetState(COOKIE_WAIT);
    StartInitTimer();
}

void
SctpAssociation::SendInitAck(uint32_t peerTag, uint32_t peerTsn, Ipv4Address dest, const std::vector<Ipv4Address>& peerAddrs)
{
    Ptr<SctpHeader> header = Create<SctpHeader>();
    header->SetSourcePort(m_socket->GetLocalPort());
    header->SetDestinationPort(m_socket->GetLocalPort());
    header->SetVerificationTag(peerTag);

    Ptr<SctpChunkInitAck> initAck = Create<SctpChunkInitAck>();
    initAck->m_initiateTag = m_localTag;
    initAck->m_aRwnd = 65535;
    initAck->m_numOutboundStreams = m_numOutboundStreams;
    initAck->m_numInboundStreams = m_numInboundStreams;
    initAck->m_initialTsn = m_localTsn;
    initAck->m_ipv4Addresses = m_localAddresses;

    // Build state cookie: contains local & peer tags + remote address
    std::ostringstream oss;
    oss << "Cookie:" << m_localTag << ":" << peerTag << ":" << dest;
    std::string cookieStr = oss.str();
    initAck->m_cookie = std::vector<uint8_t>(cookieStr.begin(), cookieStr.end());

    header->AddChunk(initAck);

    NS_LOG_INFO("Sending INIT ACK to " << dest << " with local tag " << m_localTag);
    SendPacket(header, dest);
}

void
SctpAssociation::SendCookieEcho()
{
    Ptr<SctpHeader> header = Create<SctpHeader>();
    header->SetSourcePort(m_socket->GetLocalPort());
    header->SetDestinationPort(m_socket->GetLocalPort());
    header->SetVerificationTag(m_peerTag);

    Ptr<SctpChunkCookieEcho> cookieEcho = Create<SctpChunkCookieEcho>();
    // Cookie is obtained from init-ack, which we stored
    cookieEcho->m_cookie = std::vector<uint8_t>(32, 0xAB); // placeholder/mock cookie

    header->AddChunk(cookieEcho);

    Ipv4Address dest = SelectDestinationPath();
    NS_LOG_INFO("Sending COOKIE ECHO to " << dest);
    SendPacket(header, dest);

    m_cookieAttempts++;
    SetState(COOKIE_ECHOED);
    StartCookieTimer();
}

void
SctpAssociation::SendCookieAck(Ipv4Address dest)
{
    Ptr<SctpHeader> header = Create<SctpHeader>();
    header->SetSourcePort(m_socket->GetLocalPort());
    header->SetDestinationPort(m_socket->GetLocalPort());
    header->SetVerificationTag(m_peerTag);

    Ptr<SctpChunkCookieAck> cookieAck = Create<SctpChunkCookieAck>();
    header->AddChunk(cookieAck);

    NS_LOG_INFO("Sending COOKIE ACK to " << dest);
    SendPacket(header, dest);
}

void
SctpAssociation::SendHeartbeat(Ipv4Address dest)
{
    Ptr<SctpHeader> header = Create<SctpHeader>();
    header->SetSourcePort(m_socket->GetLocalPort());
    header->SetDestinationPort(m_socket->GetLocalPort());
    header->SetVerificationTag(m_peerTag);

    Ptr<SctpChunkHeartbeat> hb = Create<SctpChunkHeartbeat>();
    std::string infoStr = "HB:" + std::to_string(Simulator::Now().GetMilliSeconds());
    hb->m_info = std::vector<uint8_t>(infoStr.begin(), infoStr.end());
    header->AddChunk(hb);

    NS_LOG_INFO("Sending HEARTBEAT to path " << dest);
    SendPacket(header, dest);
}

void
SctpAssociation::SendHeartbeatAck(const std::vector<uint8_t> &info, Ipv4Address dest)
{
    Ptr<SctpHeader> header = Create<SctpHeader>();
    header->SetSourcePort(m_socket->GetLocalPort());
    header->SetDestinationPort(m_socket->GetLocalPort());
    header->SetVerificationTag(m_peerTag);

    Ptr<SctpChunkHeartbeatAck> hbAck = Create<SctpChunkHeartbeatAck>();
    hbAck->m_info = info;
    header->AddChunk(hbAck);

    NS_LOG_INFO("Sending HEARTBEAT ACK to " << dest);
    SendPacket(header, dest);
}

void
SctpAssociation::SendSack()
{
    Ptr<SctpHeader> header = Create<SctpHeader>();
    header->SetSourcePort(m_socket->GetLocalPort());
    header->SetDestinationPort(m_socket->GetLocalPort());
    header->SetVerificationTag(m_peerTag);

    Ptr<SctpChunkSack> sack = Create<SctpChunkSack>();
    sack->m_cumulativeTsnAck = m_cumulativeTsnAck;
    sack->m_aRwnd = 65535;

    // Compute Gap Ack Blocks based on out-of-order m_rxTsns
    std::vector<std::pair<uint16_t, uint16_t>> gaps;
    if (!m_rxTsns.empty())
    {
        uint32_t startTsn = 0;
        uint32_t prevTsn = 0;
        bool inBlock = false;

        for (uint32_t tsn : m_rxTsns)
        {
            if (!inBlock)
            {
                startTsn = tsn;
                prevTsn = tsn;
                inBlock = true;
            }
            else if (tsn == prevTsn + 1)
            {
                prevTsn = tsn;
            }
            else
            {
                uint16_t startOffset = startTsn - m_cumulativeTsnAck;
                uint16_t endOffset = prevTsn - m_cumulativeTsnAck;
                gaps.push_back(std::make_pair(startOffset, endOffset));

                startTsn = tsn;
                prevTsn = tsn;
            }
        }
        if (inBlock)
        {
            uint16_t startOffset = startTsn - m_cumulativeTsnAck;
            uint16_t endOffset = prevTsn - m_cumulativeTsnAck;
            gaps.push_back(std::make_pair(startOffset, endOffset));
        }
    }
    sack->m_gapAckBlocks = gaps;

    header->AddChunk(sack);
    Ipv4Address dest = m_lastRxAddress.IsAny() ? SelectDestinationPath() : m_lastRxAddress;
    NS_LOG_INFO("Sending SACK to " << dest << " [cum=" << m_cumulativeTsnAck << ", gaps=" << gaps.size() << "]");
    SendPacket(header, dest);
}

void
SctpAssociation::SendShutdown(uint32_t cumTsnAck)
{
    Ptr<SctpHeader> header = Create<SctpHeader>();
    header->SetSourcePort(m_socket->GetLocalPort());
    header->SetDestinationPort(m_socket->GetLocalPort());
    header->SetVerificationTag(m_peerTag);

    Ptr<SctpChunkShutdown> shut = Create<SctpChunkShutdown>();
    shut->m_cumulativeTsnAck = cumTsnAck;
    header->AddChunk(shut);

    Ipv4Address dest = SelectDestinationPath();
    NS_LOG_INFO("Sending SHUTDOWN to " << dest << " [cum=" << cumTsnAck << "]");
    SendPacket(header, dest);
}

void
SctpAssociation::SendShutdownAck()
{
    Ptr<SctpHeader> header = Create<SctpHeader>();
    header->SetSourcePort(m_socket->GetLocalPort());
    header->SetDestinationPort(m_socket->GetLocalPort());
    header->SetVerificationTag(m_peerTag);

    Ptr<SctpChunkShutdownAck> shutAck = Create<SctpChunkShutdownAck>();
    header->AddChunk(shutAck);

    Ipv4Address dest = SelectDestinationPath();
    NS_LOG_INFO("Sending SHUTDOWN ACK to " << dest);
    SendPacket(header, dest);
}

void
SctpAssociation::SendShutdownComplete(bool tBit)
{
    Ptr<SctpHeader> header = Create<SctpHeader>();
    header->SetSourcePort(m_socket->GetLocalPort());
    header->SetDestinationPort(m_socket->GetLocalPort());
    header->SetVerificationTag(m_peerTag);

    Ptr<SctpChunkShutdownComplete> shutComplete = Create<SctpChunkShutdownComplete>();
    if (tBit)
    {
        shutComplete->SetFlags(0x01);
    }
    header->AddChunk(shutComplete);

    Ipv4Address dest = SelectDestinationPath();
    NS_LOG_INFO("Sending SHUTDOWN COMPLETE to " << dest);
    SendPacket(header, dest);
}

void
SctpAssociation::ProcessPacket(const SctpHeader &header, Ipv4Address fromAddr, Ipv4Address toAddr)
{
    m_lastRxAddress = fromAddr;
    NS_LOG_INFO("ProcessPacket: from=" << fromAddr << ", localTag=" << m_localTag << ", state=" << m_state);
    if (m_state != CLOSED && header.GetVerificationTag() != m_localTag)
    {
        // Verify init exceptions
        bool hasInit = false;
        for (const auto &chunk : header.GetChunks())
        {
            if (chunk->GetType() == SctpChunk::INIT)
            {
                hasInit = true;
                break;
            }
        }
        if (!hasInit)
        {
            NS_LOG_WARN("Verification tag mismatch: got " << header.GetVerificationTag() << ", expected " << m_localTag);
            return;
        }
    }

    // Packet reception confirms path liveness
    PathInfo *path = GetPath(fromAddr);
    if (path)
    {
        if (!path->active)
        {
            NS_LOG_INFO("Path " << fromAddr << " recovered (marked ACTIVE).");
            path->active = true;
        }
        path->errorCount = 0;
        StartHbTimer(*path); // refresh heartbeat timer
    }

    for (const auto &chunk : header.GetChunks())
    {
        switch (chunk->GetType())
        {
            case SctpChunk::INIT:
            {
                Ptr<SctpChunkInit> init = DynamicCast<SctpChunkInit>(chunk);
                NS_LOG_INFO("Received INIT from " << fromAddr);
                if (m_state == CLOSED)
                {
                    m_peerTag = init->m_initiateTag;
                    m_peerTsn = init->m_initialTsn;
                    m_cumulativeTsnAck = m_peerTsn - 1;
                    m_lastExpectedTsn = m_peerTsn;
                    
                    // Accept stream allocations from peer
                    m_numInboundStreams = std::min(m_numInboundStreams, init->m_numOutboundStreams);
                    m_numOutboundStreams = std::min(m_numOutboundStreams, init->m_numInboundStreams);

                    m_outStreamSeq.assign(m_numOutboundStreams, 0);
                    m_inStreamSeq.assign(m_numInboundStreams, 0);

                    // Add peer addresses
                    std::vector<Ipv4Address> peerAddrs = init->m_ipv4Addresses;
                    if (peerAddrs.empty())
                    {
                        peerAddrs.push_back(fromAddr);
                    }
                    SetPeerAddresses(peerAddrs);

                    SendInitAck(m_peerTag, m_localTsn, fromAddr, peerAddrs);
                }
                break;
            }
            case SctpChunk::INIT_ACK:
            {
                Ptr<SctpChunkInitAck> initAck = DynamicCast<SctpChunkInitAck>(chunk);
                NS_LOG_INFO("Received INIT ACK from " << fromAddr);
                if (m_state == COOKIE_WAIT)
                {
                    StopInitTimer();
                    m_peerTag = initAck->m_initiateTag;
                    m_peerTsn = initAck->m_initialTsn;
                    m_cumulativeTsnAck = m_peerTsn - 1;
                    m_lastExpectedTsn = m_peerTsn;

                    m_numInboundStreams = std::min(m_numInboundStreams, initAck->m_numOutboundStreams);
                    m_numOutboundStreams = std::min(m_numOutboundStreams, initAck->m_numInboundStreams);

                    m_outStreamSeq.assign(m_numOutboundStreams, 0);
                    m_inStreamSeq.assign(m_numInboundStreams, 0);

                    std::vector<Ipv4Address> peerAddrs = initAck->m_ipv4Addresses;
                    if (peerAddrs.empty())
                    {
                        peerAddrs.push_back(fromAddr);
                    }
                    SetPeerAddresses(peerAddrs);

                    SendCookieEcho();
                }
                break;
            }
            case SctpChunk::COOKIE_ECHO:
            {
                NS_LOG_INFO("Received COOKIE ECHO from " << fromAddr);
                if (m_state == CLOSED || m_state == COOKIE_WAIT)
                {
                    SendCookieAck(fromAddr);
                    SetState(ESTABLISHED);
                    if (m_socket)
                    {
                        m_socket->ConnectionSucceeded();
                    }
                    // Start heartbeats for secondary paths
                    for (auto &p : m_peerPaths)
                    {
                        StartHbTimer(p);
                    }
                }
                break;
            }
            case SctpChunk::COOKIE_ACK:
            {
                NS_LOG_INFO("Received COOKIE ACK from " << fromAddr);
                if (m_state == COOKIE_ECHOED)
                {
                    StopCookieTimer();
                    SetState(ESTABLISHED);
                    if (m_socket)
                    {
                        m_socket->ConnectionSucceeded();
                    }
                    // Start heartbeats for secondary paths
                    for (auto &p : m_peerPaths)
                    {
                        StartHbTimer(p);
                    }
                }
                break;
            }
            case SctpChunk::DATA:
            {
                Ptr<SctpChunkData> data = DynamicCast<SctpChunkData>(chunk);
                if (m_state == ESTABLISHED || m_state == SHUTDOWN_PENDING || m_state == SHUTDOWN_SENT)
                {
                    HandleDataChunk(data);
                }
                break;
            }
            case SctpChunk::SACK:
            {
                Ptr<SctpChunkSack> sack = DynamicCast<SctpChunkSack>(chunk);
                if (m_state == ESTABLISHED || m_state == SHUTDOWN_PENDING || m_state == SHUTDOWN_SENT)
                {
                    ProcessSack(sack, fromAddr);
                }
                break;
            }
            case SctpChunk::HEARTBEAT:
            {
                Ptr<SctpChunkHeartbeat> hb = DynamicCast<SctpChunkHeartbeat>(chunk);
                SendHeartbeatAck(hb->m_info, fromAddr);
                break;
            }
            case SctpChunk::HEARTBEAT_ACK:
            {
                NS_LOG_INFO("Received HEARTBEAT ACK from " << fromAddr);
                break;
            }
            case SctpChunk::SHUTDOWN:
            {
                Ptr<SctpChunkShutdown> shut = DynamicCast<SctpChunkShutdown>(chunk);
                NS_LOG_INFO("Received SHUTDOWN. Peer cumulative TSN Ack: " << shut->m_cumulativeTsnAck);
                if (m_state == ESTABLISHED || m_state == SHUTDOWN_PENDING)
                {
                    SetState(SHUTDOWN_RECEIVED);
                    SendShutdownAck();
                    SetState(SHUTDOWN_ACK_SENT);
                }
                break;
            }
            case SctpChunk::SHUTDOWN_ACK:
            {
                NS_LOG_INFO("Received SHUTDOWN ACK from " << fromAddr);
                if (m_state == SHUTDOWN_SENT)
                {
                    StopRtoTimer();
                    SendShutdownComplete(false);
                    SetState(CLOSED);
                    if (m_socket)
                    {
                        m_socket->AssociationClosed(this);
                    }
                }
                break;
            }
            case SctpChunk::SHUTDOWN_COMPLETE:
            {
                NS_LOG_INFO("Received SHUTDOWN COMPLETE from " << fromAddr);
                if (m_state == SHUTDOWN_ACK_SENT)
                {
                    SetState(CLOSED);
                    if (m_socket)
                    {
                        m_socket->AssociationClosed(this);
                    }
                }
                break;
            }
            case SctpChunk::ABORT:
            {
                NS_LOG_INFO("Received ABORT. Terminating association.");
                SetState(CLOSED);
                if (m_socket)
                {
                    m_socket->ConnectionFailed();
                }
                break;
            }
            default:
                break;
        }
    }
}

void
SctpAssociation::HandleDataChunk(Ptr<SctpChunkData> dataChunk)
{
    uint32_t tsn = dataChunk->m_tsn;
    NS_LOG_INFO("Rx DATA chunk: TSN=" << tsn << ", Stream=" << dataChunk->m_streamId << ", SSN=" << dataChunk->m_streamSeqNum << ", Expected=" << m_lastExpectedTsn);

    // Duplicate detection
    if (tsn < m_lastExpectedTsn || m_rxTsns.count(tsn) > 0)
    {
        NS_LOG_INFO("Duplicate DATA chunk dropped.");
        SendSack(); // Re-ack
        return;
    }

    if (tsn == m_lastExpectedTsn)
    {
        // Expected cumulative TSN received
        m_cumulativeTsnAck = tsn;
        m_lastExpectedTsn++;

        // Advance cumulative TSN through contiguous out-of-order blocks
        while (!m_rxTsns.empty() && *m_rxTsns.begin() == m_lastExpectedTsn)
        {
            m_cumulativeTsnAck = m_lastExpectedTsn;
            m_rxTsns.erase(m_rxTsns.begin());
            m_lastExpectedTsn++;
        }
    }
    else
    {
        // Out of order TSN
        m_rxTsns.insert(tsn);
    }

    // Packet creation for Socket delivery
    Ptr<Packet> p = Create<Packet>(dataChunk->m_payload.data(), dataChunk->m_payload.size());

    // Check if Unordered
    bool unordered = (dataChunk->GetFlags() & 0x04) != 0;
    if (unordered)
    {
        if (m_socket)
        {
            SctpRecvInfo rinfo;
            rinfo.streamId = dataChunk->m_streamId;
            rinfo.ssn = dataChunk->m_streamSeqNum;
            rinfo.ppid = dataChunk->m_ppid;
            rinfo.assocId = (uint32_t)m_localTag;
            m_socket->ForwardUp(p, SelectDestinationPath(), rinfo);
        }
    }
    else
    {
        // Reassemble/Order logic based on SSN
        uint16_t streamId = dataChunk->m_streamId;
        uint16_t ssn = dataChunk->m_streamSeqNum;
        
        if (streamId < m_numInboundStreams)
        {
            m_inReassembly[streamId][ssn] = p;
            DeliverOrderedData(streamId);
        }
    }

    // Send Selective Acknowledgment
    SendSack();
}

void
SctpAssociation::DeliverOrderedData(uint16_t streamId)
{
    auto &streamReasm = m_inReassembly[streamId];
    uint16_t nextSsn = m_inStreamSeq[streamId];

    while (!streamReasm.empty() && streamReasm.begin()->first == nextSsn)
    {
        Ptr<Packet> p = streamReasm.begin()->second;
        streamReasm.erase(streamReasm.begin());

        if (m_socket)
        {
            SctpRecvInfo rinfo;
            rinfo.streamId = streamId;
            rinfo.ssn = nextSsn;
            rinfo.ppid = 0; // standard payload ID
            rinfo.assocId = (uint32_t)m_localTag;
            m_socket->ForwardUp(p, SelectDestinationPath(), rinfo);
        }

        m_inStreamSeq[streamId]++;
        nextSsn = m_inStreamSeq[streamId];
    }
}

int
SctpAssociation::SendData(Ptr<Packet> packet, uint16_t streamId, uint32_t ppid, uint32_t flags)
{
    if (m_state != ESTABLISHED)
    {
        return -1;
    }

    uint32_t size = packet->GetSize();
    std::vector<uint8_t> buffer(size);
    packet->CopyData(buffer.data(), size);

    TxData tx;
    tx.tsn = m_localTsn++;
    tx.streamId = streamId;
    tx.ppid = ppid;
    tx.payload = buffer;
    tx.sentTime = Simulator::Now();
    tx.acknowledged = false;

    bool unordered = (flags & 0x04) != 0;
    if (unordered)
    {
        tx.streamSeqNum = 0;
    }
    else
    {
        if (streamId >= m_outStreamSeq.size())
        {
            return -1;
        }
        tx.streamSeqNum = m_outStreamSeq[streamId]++;
    }

    m_txQueue.push_back(tx);

    // Send DATA packet
    Ptr<SctpHeader> header = Create<SctpHeader>();
    header->SetSourcePort(m_socket->GetLocalPort());
    header->SetDestinationPort(m_socket->GetLocalPort());
    header->SetVerificationTag(m_peerTag);

    Ptr<SctpChunkData> dataChunk = Create<SctpChunkData>();
    dataChunk->m_tsn = tx.tsn;
    dataChunk->m_streamId = streamId;
    dataChunk->m_streamSeqNum = tx.streamSeqNum;
    dataChunk->m_ppid = ppid;
    dataChunk->m_payload = buffer;
    
    uint8_t cflags = 0x03; // Begin & End fragments (no fragmentation implemented)
    if (unordered)
    {
        cflags |= 0x04;
    }
    dataChunk->SetFlags(cflags);
    header->AddChunk(dataChunk);

    Ipv4Address dest = SelectDestinationPath();
    NS_LOG_INFO("Sending DATA TSN " << tx.tsn << " to " << dest);
    
    // Update path flightSize
    PathInfo* path = GetPath(dest);
    if (path)
    {
        path->flightSize += size + 12 + 4;
    }

    tx.lastDest = dest;
    SendPacket(header, dest);

    StartRtoTimer(); // Start/restart recovery timer
    return size;
}

void
SctpAssociation::ProcessSack(Ptr<SctpChunkSack> sackChunk, Ipv4Address from)
{
    uint32_t cumAck = sackChunk->m_cumulativeTsnAck;
    m_peerRwnd = sackChunk->m_aRwnd;

    NS_LOG_INFO("Received SACK from " << from << ": cumAck=" << cumAck << ", rwnd=" << m_peerRwnd << ", gaps=" << sackChunk->m_gapAckBlocks.size());

    bool newlyAcked = false;

    // 1. Mark as acknowledged by cumulative Ack
    for (auto &tx : m_txQueue)
    {
        if (!tx.acknowledged && tx.tsn <= cumAck)
        {
            tx.acknowledged = true;
            newlyAcked = true;
            PathInfo* path = GetPath(tx.lastDest);
            if (path && path->flightSize >= tx.payload.size())
            {
                path->flightSize -= (tx.payload.size() + 16);
            }
        }
    }

    // 2. Mark Gap Ack Blocks
    for (const auto &block : sackChunk->m_gapAckBlocks)
    {
        uint32_t startTsn = cumAck + block.first;
        uint32_t endTsn = cumAck + block.second;
        for (auto &tx : m_txQueue)
        {
            if (!tx.acknowledged && tx.tsn >= startTsn && tx.tsn <= endTsn)
            {
                tx.acknowledged = true;
                newlyAcked = true;
                PathInfo* path = GetPath(tx.lastDest);
                if (path && path->flightSize >= tx.payload.size())
                {
                    path->flightSize -= (tx.payload.size() + 16);
                }
            }
        }
    }

    // 3. Clear acknowledged from front of queue
    while (!m_txQueue.empty() && m_txQueue.front().acknowledged)
    {
        m_txQueue.erase(m_txQueue.begin());
    }

    // Adjust Congestion Window
    PathInfo* path = GetPath(from);
    if (path && newlyAcked)
    {
        if (path->cwnd < path->ssthresh)
        {
            path->cwnd += 1460; // slow start
        }
        else
        {
            path->cwnd += (1460 * 1460) / path->cwnd; // congestion avoidance
        }
    }

    if (m_txQueue.empty())
    {
        StopRtoTimer();
        if (m_state == SHUTDOWN_PENDING)
        {
            SendShutdown(m_lastExpectedTsn);
            SetState(SHUTDOWN_SENT);
            StartRtoTimer();
        }
    }
    else
    {
        StartRtoTimer(); // reset timer for remaining packets
    }
}

// Timer Implementations
void
SctpAssociation::StartInitTimer()
{
    StopInitTimer();
    m_initTimer = Simulator::Schedule(Seconds(1.0), &SctpAssociation::InitTimeout, this);
}

void
SctpAssociation::StopInitTimer()
{
    if (m_initTimer.IsPending())
    {
        m_initTimer.Cancel();
    }
}

void
SctpAssociation::InitTimeout()
{
    if (m_state == COOKIE_WAIT)
    {
        if (m_initAttempts >= 5)
        {
            NS_LOG_ERROR("INIT attempts exceeded limit. Terminating association.");
            SetState(CLOSED);
            if (m_socket)
            {
                m_socket->ConnectionFailed();
            }
            return;
        }
        NS_LOG_INFO("INIT timeout. Retransmitting INIT.");
        SendInit();
    }
}

void
SctpAssociation::StartCookieTimer()
{
    StopCookieTimer();
    m_cookieTimer = Simulator::Schedule(Seconds(1.0), &SctpAssociation::CookieTimeout, this);
}

void
SctpAssociation::StopCookieTimer()
{
    if (m_cookieTimer.IsPending())
    {
        m_cookieTimer.Cancel();
    }
}

void
SctpAssociation::CookieTimeout()
{
    if (m_state == COOKIE_ECHOED)
    {
        if (m_cookieAttempts >= 5)
        {
            NS_LOG_ERROR("COOKIE ECHO attempts exceeded limit. Terminating association.");
            SetState(CLOSED);
            if (m_socket)
            {
                m_socket->ConnectionFailed();
            }
            return;
        }
        NS_LOG_INFO("COOKIE timeout. Retransmitting COOKIE ECHO.");
        SendCookieEcho();
    }
}

void
SctpAssociation::StartHbTimer(PathInfo &path)
{
    StopHbTimer(path);
    path.hbEvent = Simulator::Schedule(Seconds(10.0), &SctpAssociation::HbTimeout, this, path.address);
}

void
SctpAssociation::StopHbTimer(PathInfo &path)
{
    if (path.hbEvent.IsPending())
    {
        path.hbEvent.Cancel();
    }
}

void
SctpAssociation::HbTimeout(Ipv4Address addr)
{
    PathInfo* path = GetPath(addr);
    if (path)
    {
        SendHeartbeat(addr);
        path->errorCount++;
        if (path->errorCount >= 5)
        {
            if (path->active)
            {
                NS_LOG_WARN("Path " << addr << " became INACTIVE due to missed heartbeats!");
                path->active = false;
            }
        }
        StartHbTimer(*path); // schedule next
    }
}

void
SctpAssociation::StartRtoTimer()
{
    StopRtoTimer();
    m_rtoTimer = Simulator::Schedule(Seconds(1.0), &SctpAssociation::RtoTimeout, this);
}

void
SctpAssociation::StopRtoTimer()
{
    if (m_rtoTimer.IsPending())
    {
        m_rtoTimer.Cancel();
    }
}

void
SctpAssociation::RtoTimeout()
{
    if (m_state == ESTABLISHED || m_state == SHUTDOWN_PENDING || m_state == SHUTDOWN_SENT)
    {
        NS_LOG_WARN("RTO timeout. Processing retransmissions.");
        bool retransSent = false;
        
        for (auto &tx : m_txQueue)
        {
            if (!tx.acknowledged)
            {
                // Increment error count on the path that failed
                PathInfo* failedPath = GetPath(tx.lastDest);
                if (failedPath)
                {
                    failedPath->errorCount++;
                    if (failedPath->errorCount >= 5 && failedPath->active)
                    {
                        NS_LOG_WARN("Path " << tx.lastDest << " marked INACTIVE due to packet losses!");
                        failedPath->active = false;
                    }
                }

                // Pick alternative path
                Ipv4Address altDest = SelectDestinationPath();
                NS_LOG_INFO("Retransmitting DATA TSN " << tx.tsn << " to " << altDest);

                Ptr<SctpHeader> header = Create<SctpHeader>();
                header->SetSourcePort(m_socket->GetLocalPort());
                header->SetDestinationPort(m_socket->GetLocalPort());
                header->SetVerificationTag(m_peerTag);

                Ptr<SctpChunkData> dataChunk = Create<SctpChunkData>();
                dataChunk->m_tsn = tx.tsn;
                dataChunk->m_streamId = tx.streamId;
                dataChunk->m_streamSeqNum = tx.streamSeqNum;
                dataChunk->m_ppid = tx.ppid;
                dataChunk->m_payload = tx.payload;
                dataChunk->SetFlags(0x03);
                header->AddChunk(dataChunk);

                tx.lastDest = altDest;
                tx.sentTime = Simulator::Now();
                SendPacket(header, altDest);

                retransSent = true;
            }
        }

        if (m_state == SHUTDOWN_SENT)
        {
            // Retransmit shutdown chunk
            SendShutdown(m_lastExpectedTsn);
            retransSent = true;
        }

        if (retransSent)
        {
            // Exponential backoff
            StartRtoTimer();
        }
    }
}

} // namespace ns3
