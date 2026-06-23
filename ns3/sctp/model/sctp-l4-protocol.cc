#include "sctp-l4-protocol.h"
#include "ns3/log.h"
#include "ns3/node.h"
#include "ns3/ipv4.h"
#include "ns3/ipv4-routing-protocol.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("SctpL4Protocol");

NS_OBJECT_ENSURE_REGISTERED(SctpSocketFactory);
NS_OBJECT_ENSURE_REGISTERED(SctpSocketFactoryImpl);
NS_OBJECT_ENSURE_REGISTERED(SctpL4Protocol);

TypeId
SctpSocketFactory::GetTypeId()
{
    static TypeId tid = TypeId("ns3::SctpSocketFactory")
                            .SetParent<SocketFactory>()
                            .SetGroupName("Sctp");
    return tid;
}

TypeId
SctpSocketFactoryImpl::GetTypeId()
{
    static TypeId tid = TypeId("ns3::SctpSocketFactoryImpl")
                            .SetParent<SctpSocketFactory>()
                            .SetGroupName("Sctp")
                            .AddConstructor<SctpSocketFactoryImpl>();
    return tid;
}

SctpSocketFactoryImpl::SctpSocketFactoryImpl()
    : m_sctp(nullptr)
{
}

SctpSocketFactoryImpl::SctpSocketFactoryImpl(Ptr<SctpL4Protocol> sctp)
    : m_sctp(sctp)
{
}

SctpSocketFactoryImpl::~SctpSocketFactoryImpl()
{
}

Ptr<Socket>
SctpSocketFactoryImpl::CreateSocket()
{
    if (m_sctp)
    {
        return m_sctp->CreateSocket();
    }
    return nullptr;
}

TypeId
SctpL4Protocol::GetTypeId()
{
    static TypeId tid = TypeId("ns3::SctpL4Protocol")
                            .SetParent<IpL4Protocol>()
                            .SetGroupName("Sctp")
                            .AddConstructor<SctpL4Protocol>();
    return tid;
}

SctpL4Protocol::SctpL4Protocol()
    : m_node(nullptr),
      m_lastPort(49152)
{
}

SctpL4Protocol::~SctpL4Protocol()
{
}

void
SctpL4Protocol::NotifyNewAggregate()
{
    if (m_node == nullptr)
    {
        Ptr<Node> node = this->GetObject<Node>();
        if (node != nullptr)
        {
            m_node = node;
            Ptr<Ipv4> ipv4 = m_node->GetObject<Ipv4>();
            if (ipv4 != nullptr)
            {
                ipv4->Insert(this);
            }
            // Aggregate our SctpSocketFactoryImpl to the Node!
            Ptr<SctpSocketFactoryImpl> factory = CreateObject<SctpSocketFactoryImpl>(this);
            m_node->AggregateObject(factory);
        }
    }
    IpL4Protocol::NotifyNewAggregate();
}

void
SctpL4Protocol::DoDispose()
{
    m_sockets.clear();
    m_associations.clear();
    m_node = nullptr;
    IpL4Protocol::DoDispose();
}

int
SctpL4Protocol::GetProtocolNumber() const
{
    return PROT_NUMBER;
}

IpL4Protocol::RxStatus
SctpL4Protocol::Receive(Ptr<Packet> p, const Ipv4Header &header, Ptr<Ipv4Interface> interface)
{
    SctpHeader sctpHeader;
    if (p->GetSize() < sctpHeader.GetSerializedSize())
    {
        NS_LOG_WARN("Packet too small to contain SCTP header.");
        return IpL4Protocol::RX_CSUM_FAILED;
    }

    p->RemoveHeader(sctpHeader);

    uint16_t localPort = sctpHeader.GetDestinationPort();
    uint16_t peerPort = sctpHeader.GetSourcePort();
    uint32_t vtag = sctpHeader.GetVerificationTag();

    NS_LOG_INFO("Rx SCTP packet from " << header.GetSource() << ": srcPort=" << peerPort << ", dstPort=" << localPort << ", vtag=" << vtag);

    // 1. Search for matching active association
    for (auto &assoc : m_associations)
    {
        if (assoc->GetSocket() && assoc->GetSocket()->GetLocalPort() == localPort)
        {
            assoc->GetSocket()->m_peerPort = peerPort;
            assoc->ProcessPacket(sctpHeader, header.GetSource(), header.GetDestination());
            return IpL4Protocol::RX_OK;
        }
    }

    // 2. No active association, search for a listening socket on localPort
    for (auto &socket : m_sockets)
    {
        if (socket->GetLocalPort() == localPort && socket->m_isListening) // check listening
        {
            // Check if packet contains an INIT chunk
            for (const auto &chunk : sctpHeader.GetChunks())
            {
                if (chunk->GetType() == SctpChunk::INIT)
                {
                    NS_LOG_INFO("Accepting new SCTP connection request on port " << localPort);
                    Ptr<SctpAssociation> assoc = CreateObject<SctpAssociation>();
                    assoc->SetSocket(socket);
                    socket->m_association = assoc;

                    // Configure local and peer addresses for the new association
                    assoc->SetLocalAddresses(socket->GetLocalAddresses());
                    std::vector<Ipv4Address> peerAddrs;
                    peerAddrs.push_back(header.GetSource());
                    assoc->SetPeerAddresses(peerAddrs);
                    socket->m_peerAddresses = peerAddrs;
                    socket->m_peerPort = peerPort;

                    RegisterAssociation(assoc);
                    assoc->ProcessPacket(sctpHeader, header.GetSource(), header.GetDestination());
                    return IpL4Protocol::RX_OK;
                }
            }
        }
    }

    NS_LOG_WARN("No matching association or listening socket on port " << localPort);
    return IpL4Protocol::RX_ENDPOINT_UNREACH;
}

IpL4Protocol::RxStatus
SctpL4Protocol::Receive(Ptr<Packet> p, const Ipv6Header &header, Ptr<Ipv6Interface> interface)
{
    return IpL4Protocol::RX_ENDPOINT_UNREACH; // IPv6 not implemented
}

Ptr<Socket>
SctpL4Protocol::CreateSocket()
{
    Ptr<SctpSocket> socket = CreateObject<SctpSocket>();
    socket->SetNode(m_node);
    socket->SetIpL4Protocol(this);
    RegisterSocket(socket);
    return socket;
}

uint16_t
SctpL4Protocol::AllocatePort(Ptr<SctpSocket> socket)
{
    uint16_t port = m_lastPort++;
    if (m_lastPort > 65535)
    {
        m_lastPort = 49152;
    }
    RegisterSocket(socket);
    return port;
}

void
SctpL4Protocol::RegisterSocket(Ptr<SctpSocket> socket)
{
    for (const auto &s : m_sockets)
    {
        if (s == socket)
        {
            return;
        }
    }
    m_sockets.push_back(socket);
}

void
SctpL4Protocol::DeallocateSocket(Ptr<SctpSocket> socket)
{
    for (auto it = m_sockets.begin(); it != m_sockets.end(); ++it)
    {
        if (*it == socket)
        {
            m_sockets.erase(it);
            break;
        }
    }
}

void
SctpL4Protocol::RegisterAssociation(Ptr<SctpAssociation> assoc)
{
    for (const auto &a : m_associations)
    {
        if (a == assoc)
        {
            return;
        }
    }
    m_associations.push_back(assoc);
}

void
SctpL4Protocol::DeallocateAssociation(Ptr<SctpAssociation> assoc)
{
    for (auto it = m_associations.begin(); it != m_associations.end(); ++it)
    {
        if (*it == assoc)
        {
            m_associations.erase(it);
            break;
        }
    }
}

void
SctpL4Protocol::SendPacket(Ptr<Packet> packet, Ipv4Address source, Ipv4Address destination)
{
    if (m_node == nullptr)
    {
        return;
    }
    Ptr<Ipv4> ipv4 = m_node->GetObject<Ipv4>();
    if (ipv4 == nullptr)
    {
        return;
    }
    Ptr<Ipv4RoutingProtocol> rp = ipv4->GetRoutingProtocol();
    if (rp == nullptr)
    {
        return;
    }

    Ipv4Header ipHeader;
    ipHeader.SetDestination(destination);
    if (!source.IsAny())
    {
        ipHeader.SetSource(source);
    }

    Socket::SocketErrno sockerr;
    Ptr<Ipv4Route> route = rp->RouteOutput(packet, ipHeader, nullptr, sockerr);
    if (route != nullptr)
    {
        Ipv4Address src = source.IsAny() ? route->GetSource() : source;
        ipv4->Send(packet, src, destination, PROT_NUMBER, route);
    }
    else
    {
        NS_LOG_ERROR("No route to destination " << destination);
    }
}

void
SctpL4Protocol::SetDownTarget(DownTargetCallback cb)
{
    m_downTarget = cb;
}

void
SctpL4Protocol::SetDownTarget6(DownTargetCallback6 cb)
{
    m_downTarget6 = cb;
}

IpL4Protocol::DownTargetCallback
SctpL4Protocol::GetDownTarget() const
{
    return m_downTarget;
}

IpL4Protocol::DownTargetCallback6
SctpL4Protocol::GetDownTarget6() const
{
    return m_downTarget6;
}

} // namespace ns3
