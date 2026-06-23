#ifndef SCTP_SOCKET_H
#define SCTP_SOCKET_H

#include "ns3/socket.h"
#include "ns3/ipv4-address.h"
#include "ns3/ip-l4-protocol.h"
#include "sctp-association.h"
#include <vector>
#include <queue>

namespace ns3
{

/**
 * @ingroup sctp
 * @brief Send Info struct for SendMsg (RFC 6458)
 */
struct SctpSendInfo
{
    uint16_t streamId{0};      ///< Stream Identifier
    uint32_t ppid{0};          ///< Payload Protocol Identifier
    uint32_t flags{0};         ///< Flags (e.g. unordered, abort, shutdown)
    uint32_t assocId{0};       ///< Association ID (for one-to-many style)
};

/**
 * @ingroup sctp
 * @brief Recv Info struct for RecvMsg (RFC 6458)
 */
struct SctpRecvInfo
{
    uint16_t streamId{0};      ///< Stream Identifier
    uint16_t ssn{0};           ///< Stream Sequence Number
    uint32_t ppid{0};          ///< Payload Protocol Identifier
    uint32_t assocId{0};       ///< Association ID
};

/**
 * @ingroup sctp
 * @brief Socket interface extensions for SCTP (RFC 6458)
 */
class SctpSocket : public Socket
{
    friend class SctpL4Protocol;
    friend class SctpAssociation;

  public:
    static TypeId GetTypeId();

    SctpSocket();
    ~SctpSocket() override;

    // RFC 6458 Extensions
    /**
     * @brief Bind to multiple local IP addresses
     * @param addresses Local addresses vector
     * @param port Local port
     * @return 0 on success, -1 on failure
     */
    int Bindx(const std::vector<Ipv4Address> &addresses, uint16_t port);

    /**
     * @brief Connect to a multi-homed remote peer
     * @param addresses Remote addresses vector
     * @param port Remote port
     * @return 0 on success, -1 on failure
     */
    int Connectx(const std::vector<Ipv4Address> &addresses, uint16_t port);

    /**
     * @brief Send a message specifying SCTP parameters
     * @param packet Data packet
     * @param sendInfo Send options
     * @return Number of bytes sent, or -1 on error
     */
    int SendMsg(Ptr<Packet> packet, SctpSendInfo sendInfo);

    /**
     * @brief Receive a message along with its SCTP metadata
     * @param packet Packet to read into
     * @param recvInfo Recv metadata output
     * @return Number of bytes read, or -1 on error
     */
    int RecvMsg(Ptr<Packet> packet, SctpRecvInfo &recvInfo);

    // Standard Socket Overrides
    int Bind() override;
    int Bind(const Address &address) override;
    int Bind6() override;
    int Connect(const Address &address) override;
    int Listen() override;
    int Close() override;
    int ShutdownSend() override;
    int ShutdownRecv() override;
    int Send(Ptr<Packet> p, uint32_t flags) override;
    int SendTo(Ptr<Packet> p, uint32_t flags, const Address &toAddress) override;
    Ptr<Packet> Recv(uint32_t maxSize, uint32_t flags) override;
    Ptr<Packet> RecvFrom(uint32_t maxSize, uint32_t flags, Address &fromAddress) override;
    uint32_t GetTxAvailable() const override;
    uint32_t GetRxAvailable() const override;
    int GetSockName(Address &address) const override;
    int GetPeerName(Address &address) const override;
    void SetNode(Ptr<Node> node);
    Ptr<Node> GetNode() const override;
    void SetIpL4Protocol(Ptr<IpL4Protocol> protocol);

    // Required Pure Virtual Overrides
    SocketErrno GetErrno() const override;
    SocketType GetSocketType() const override;
    bool SetAllowBroadcast(bool allowBroadcast) override;
    bool GetAllowBroadcast() const override;

    // Callbacks from SctpAssociation
    void ForwardUp(Ptr<Packet> packet, Ipv4Address from, SctpRecvInfo recvInfo);
    void ConnectionSucceeded();
    void ConnectionFailed();
    void AssociationClosed(Ptr<SctpAssociation> assoc);

    // Get list of bound local addresses
    const std::vector<Ipv4Address>& GetLocalAddresses() const;

    // Get port
    uint16_t GetLocalPort() const;

  private:
    // Internal queue entry for received packets
    struct QueuedPacket
    {
        Ptr<Packet> packet;
        Ipv4Address from;
        SctpRecvInfo recvInfo;
    };

    Ptr<Node> m_node;                                   ///< Associated node
    Ptr<IpL4Protocol> m_l4Proto;                        ///< SCTP L4 Protocol reference
    uint16_t m_localPort;                               ///< Local port
    uint16_t m_peerPort;                                ///< Remote port
    std::vector<Ipv4Address> m_localAddresses;          ///< Bound local IPs
    std::vector<Ipv4Address> m_peerAddresses;           ///< Peer IPs

    Ptr<SctpAssociation> m_association;                 ///< SctpAssociation reference (one-to-one)
    bool m_isListening;                                 ///< If socket is listening

    std::queue<QueuedPacket> m_rxQueue;                 ///< Receive queue
    uint32_t m_rxAvailableBytes;                        ///< Total bytes in rx queue

    // Attributes
    uint16_t m_defaultOutStreams;                       ///< Default outbound streams count
    uint16_t m_defaultInStreams;                        ///< Default inbound streams count

    mutable SocketErrno m_errno;                        ///< Last socket error code
};

} // namespace ns3

#endif // SCTP_SOCKET_H
