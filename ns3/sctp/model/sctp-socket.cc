#include "sctp-socket.h"
#include "sctp-l4-protocol.h"
#include "ns3/uinteger.h"
#include "ns3/log.h"
#include "ns3/node.h"
#include "ns3/ipv4.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("SctpSocket");

NS_OBJECT_ENSURE_REGISTERED(SctpSocket);

TypeId
SctpSocket::GetTypeId()
{
    static TypeId tid = TypeId("ns3::SctpSocket")
                            .SetParent<Socket>()
                            .SetGroupName("Sctp")
                            .AddConstructor<SctpSocket>()
                            .AddAttribute("DefaultOutboundStreams",
                                          "Default number of outbound streams.",
                                          UintegerValue(2),
                                          MakeUintegerAccessor(&SctpSocket::m_defaultOutStreams),
                                          MakeUintegerChecker<uint16_t>())
                            .AddAttribute("DefaultInboundStreams",
                                          "Default number of inbound streams.",
                                          UintegerValue(2),
                                          MakeUintegerAccessor(&SctpSocket::m_defaultInStreams),
                                          MakeUintegerChecker<uint16_t>());
    return tid;
}

SctpSocket::SctpSocket()
    : m_node(nullptr),
      m_l4Proto(nullptr),
      m_localPort(0),
      m_peerPort(0),
      m_association(nullptr),
      m_isListening(false),
      m_rxAvailableBytes(0),
      m_defaultOutStreams(2),
      m_defaultInStreams(2),
      m_errno(ERROR_NOTERROR)
{
}

SctpSocket::~SctpSocket()
{
}

void
SctpSocket::SetNode(Ptr<Node> node)
{
    m_node = node;
}

Ptr<Node>
SctpSocket::GetNode() const
{
    return m_node;
}

void
SctpSocket::SetIpL4Protocol(Ptr<IpL4Protocol> protocol)
{
    m_l4Proto = protocol;
}

int
SctpSocket::Bindx(const std::vector<Ipv4Address> &addresses, uint16_t port)
{
    if (addresses.empty())
    {
        return -1;
    }
    m_localAddresses = addresses;
    m_localPort = port;
    if (m_localPort == 0)
    {
        // Allocate a dynamic port if not specified
        Ptr<SctpL4Protocol> sctpL4 = DynamicCast<SctpL4Protocol>(m_l4Proto);
        if (sctpL4)
        {
            m_localPort = sctpL4->AllocatePort(this);
        }
    }
    else
    {
        Ptr<SctpL4Protocol> sctpL4 = DynamicCast<SctpL4Protocol>(m_l4Proto);
        if (sctpL4)
        {
            sctpL4->RegisterSocket(this);
        }
    }
    return 0;
}

int
SctpSocket::Connectx(const std::vector<Ipv4Address> &addresses, uint16_t port)
{
    if (addresses.empty())
    {
        return -1;
    }
    m_peerAddresses = addresses;
    m_peerPort = port;

    // Create association
    m_association = CreateObject<SctpAssociation>();
    m_association->SetSocket(this);
    m_association->SetNumOutboundStreams(m_defaultOutStreams);
    m_association->SetNumInboundStreams(m_defaultInStreams);
    m_association->SetLocalAddresses(m_localAddresses);
    m_association->SetPeerAddresses(m_peerAddresses);
    
    // Register association in the L4 protocol registry
    Ptr<SctpL4Protocol> sctpL4 = DynamicCast<SctpL4Protocol>(m_l4Proto);
    if (sctpL4)
    {
        sctpL4->RegisterAssociation(m_association);
    }

    m_association->Initiate();
    return 0;
}

int
SctpSocket::SendMsg(Ptr<Packet> packet, SctpSendInfo sendInfo)
{
    if (!m_association)
    {
        return -1;
    }
    return m_association->SendData(packet, sendInfo.streamId, sendInfo.ppid, sendInfo.flags);
}

int
SctpSocket::RecvMsg(Ptr<Packet> packet, SctpRecvInfo &recvInfo)
{
    if (m_rxQueue.empty())
    {
        return -1;
    }
    QueuedPacket qp = m_rxQueue.front();
    m_rxQueue.pop();
    
    uint32_t size = qp.packet->GetSize();
    m_rxAvailableBytes -= size;

    packet->AddAtEnd(qp.packet);
    recvInfo = qp.recvInfo;
    return size;
}

int
SctpSocket::Bind()
{
    m_localAddresses.push_back(Ipv4Address::GetAny());
    Ptr<SctpL4Protocol> sctpL4 = DynamicCast<SctpL4Protocol>(m_l4Proto);
    if (sctpL4)
    {
        m_localPort = sctpL4->AllocatePort(this);
    }
    return 0;
}

int
SctpSocket::Bind(const Address &address)
{
    InetSocketAddress inetAddr = InetSocketAddress::ConvertFrom(address);
    m_localAddresses.push_back(inetAddr.GetIpv4());
    m_localPort = inetAddr.GetPort();
    
    Ptr<SctpL4Protocol> sctpL4 = DynamicCast<SctpL4Protocol>(m_l4Proto);
    if (sctpL4)
    {
        sctpL4->RegisterSocket(this);
    }
    return 0;
}

int
SctpSocket::Bind6()
{
    return -1; // IPv6 not implemented
}

int
SctpSocket::Connect(const Address &address)
{
    InetSocketAddress inetAddr = InetSocketAddress::ConvertFrom(address);
    std::vector<Ipv4Address> addrs;
    addrs.push_back(inetAddr.GetIpv4());
    m_peerPort = inetAddr.GetPort();
    return Connectx(addrs, inetAddr.GetPort());
}

int
SctpSocket::Listen()
{
    m_isListening = true;
    Ptr<SctpL4Protocol> sctpL4 = DynamicCast<SctpL4Protocol>(m_l4Proto);
    if (sctpL4)
    {
        sctpL4->RegisterSocket(this);
    }
    return 0;
}

int
SctpSocket::Close()
{
    if (m_association)
    {
        m_association->Close();
    }
    return 0;
}

int
SctpSocket::ShutdownSend()
{
    return Close();
}

int
SctpSocket::ShutdownRecv()
{
    return 0;
}

int
SctpSocket::Send(Ptr<Packet> p, uint32_t flags)
{
    SctpSendInfo sinfo;
    sinfo.streamId = 0;
    sinfo.ppid = 0;
    sinfo.flags = flags;
    return SendMsg(p, sinfo);
}

int
SctpSocket::SendTo(Ptr<Packet> p, uint32_t flags, const Address &toAddress)
{
    if (!m_association)
    {
        Connect(toAddress);
    }
    return Send(p, flags);
}

Ptr<Packet>
SctpSocket::Recv(uint32_t maxSize, uint32_t flags)
{
    if (m_rxQueue.empty())
    {
        return nullptr;
    }
    Ptr<Packet> packet = Create<Packet>();
    SctpRecvInfo rinfo;
    RecvMsg(packet, rinfo);
    return packet;
}

Ptr<Packet>
SctpSocket::RecvFrom(uint32_t maxSize, uint32_t flags, Address &fromAddress)
{
    if (m_rxQueue.empty())
    {
        return nullptr;
    }
    QueuedPacket qp = m_rxQueue.front();
    fromAddress = InetSocketAddress(qp.from, m_peerPort);
    
    Ptr<Packet> packet = Create<Packet>();
    SctpRecvInfo rinfo;
    RecvMsg(packet, rinfo);
    return packet;
}

uint32_t
SctpSocket::GetTxAvailable() const
{
    return 65535; // default buffer size
}

uint32_t
SctpSocket::GetRxAvailable() const
{
    return m_rxAvailableBytes;
}

int
SctpSocket::GetSockName(Address &address) const
{
    Ipv4Address local = m_localAddresses.empty() ? Ipv4Address::GetAny() : m_localAddresses[0];
    address = InetSocketAddress(local, m_localPort);
    return 0;
}

int
SctpSocket::GetPeerName(Address &address) const
{
    Ipv4Address peer = m_peerAddresses.empty() ? Ipv4Address::GetAny() : m_peerAddresses[0];
    address = InetSocketAddress(peer, m_peerPort);
    return 0;
}

void
SctpSocket::ForwardUp(Ptr<Packet> packet, Ipv4Address from, SctpRecvInfo recvInfo)
{
    QueuedPacket qp;
    qp.packet = packet;
    qp.from = from;
    qp.recvInfo = recvInfo;

    m_rxQueue.push(qp);
    m_rxAvailableBytes += packet->GetSize();

    // Trigger Rx callback to notify application
    NotifyDataRecv();
}

void
SctpSocket::ConnectionSucceeded()
{
    NS_LOG_INFO("ConnectionSucceeded called on socket " << this);
    NotifyConnectionSucceeded();
}

void
SctpSocket::ConnectionFailed()
{
    NotifyConnectionFailed();
}

void
SctpSocket::AssociationClosed(Ptr<SctpAssociation> assoc)
{
    NotifyNormalClose();
    m_association = nullptr;
}

const std::vector<Ipv4Address>&
SctpSocket::GetLocalAddresses() const
{
    return m_localAddresses;
}

uint16_t
SctpSocket::GetLocalPort() const
{
    return m_localPort;
}

Socket::SocketErrno
SctpSocket::GetErrno() const
{
    return m_errno;
}

Socket::SocketType
SctpSocket::GetSocketType() const
{
    return Socket::NS3_SOCK_SEQPACKET;
}

bool
SctpSocket::SetAllowBroadcast(bool allowBroadcast)
{
    return false;
}

bool
SctpSocket::GetAllowBroadcast() const
{
    return false;
}

} // namespace ns3
