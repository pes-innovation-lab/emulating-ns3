#ifndef SCTP_L4_PROTOCOL_H
#define SCTP_L4_PROTOCOL_H

#include "ns3/ip-l4-protocol.h"
#include "ns3/socket-factory.h"
#include "ns3/ipv4-address.h"
#include "sctp-socket.h"
#include "sctp-association.h"
#include <vector>

namespace ns3
{

/**
 * @ingroup sctp
 * @brief Socket Factory for SCTP
 */
class SctpSocketFactory : public SocketFactory
{
  public:
    static TypeId GetTypeId();
};

class SctpL4Protocol;

/**
 * @ingroup sctp
 * @brief Concrete Socket Factory Implementation for SCTP
 */
class SctpSocketFactoryImpl : public SctpSocketFactory
{
  public:
    static TypeId GetTypeId();
    SctpSocketFactoryImpl();
    SctpSocketFactoryImpl(Ptr<SctpL4Protocol> sctp);
    ~SctpSocketFactoryImpl() override;

    Ptr<Socket> CreateSocket() override;

  private:
    Ptr<SctpL4Protocol> m_sctp;                         ///< Reference to the SCTP L4 Protocol
};

/**
 * @ingroup sctp
 * @brief Native SCTP L4 Protocol implementation for ns-3 (Protocol 132)
 */
class SctpL4Protocol : public IpL4Protocol
{
  public:
    static TypeId GetTypeId();
    static constexpr uint8_t PROT_NUMBER = 132;

    SctpL4Protocol();
    ~SctpL4Protocol() override;

    // Inherited from Object
    void NotifyNewAggregate() override;
    void DoDispose() override;

    // Inherited from IpL4Protocol
    int GetProtocolNumber() const override;
    IpL4Protocol::RxStatus Receive(Ptr<Packet> p,
                                   const Ipv4Header &header,
                                   Ptr<Ipv4Interface> interface) override;
    IpL4Protocol::RxStatus Receive(Ptr<Packet> p,
                                   const Ipv6Header &header,
                                   Ptr<Ipv6Interface> interface) override;
    void SetDownTarget(DownTargetCallback cb) override;
    void SetDownTarget6(DownTargetCallback6 cb) override;
    DownTargetCallback GetDownTarget() const override;
    DownTargetCallback6 GetDownTarget6() const override;

    // Create Socket method
    Ptr<Socket> CreateSocket();

    // Socket registry methods
    uint16_t AllocatePort(Ptr<SctpSocket> socket);
    void RegisterSocket(Ptr<SctpSocket> socket);
    void DeallocateSocket(Ptr<SctpSocket> socket);

    // Association registry methods
    void RegisterAssociation(Ptr<SctpAssociation> assoc);
    void DeallocateAssociation(Ptr<SctpAssociation> assoc);

    // Send packet helper (routes and sends via Ipv4L3Protocol)
    void SendPacket(Ptr<Packet> packet, Ipv4Address source, Ipv4Address destination);

  private:
    Ptr<Node> m_node;                                   ///< The node this protocol is running on
    std::vector<Ptr<SctpSocket>> m_sockets;             ///< Registered sockets
    std::vector<Ptr<SctpAssociation>> m_associations;   ///< Active associations
    uint16_t m_lastPort;                                ///< Last allocated ephemeral port
    IpL4Protocol::DownTargetCallback m_downTarget;      ///< Down target callback
    IpL4Protocol::DownTargetCallback6 m_downTarget6;    ///< Down target callback (v6)
};

} // namespace ns3

#endif // SCTP_L4_PROTOCOL_H
