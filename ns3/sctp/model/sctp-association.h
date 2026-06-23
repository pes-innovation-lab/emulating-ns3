#ifndef SCTP_ASSOCIATION_H
#define SCTP_ASSOCIATION_H

#include "ns3/ptr.h"
#include "ns3/object.h"
#include "ns3/ipv4-address.h"
#include "ns3/event-id.h"
#include "sctp-header.h"
#include <vector>
#include <map>
#include <set>
#include <iostream>

namespace ns3
{

class SctpSocket;

/**
 * @ingroup sctp
 * @brief Representation of an SCTP Association
 */
class SctpAssociation : public Object
{
  public:
    /**
     * @brief SCTP Association States
     */
    enum State
    {
        CLOSED,
        COOKIE_WAIT,
        COOKIE_ECHOED,
        ESTABLISHED,
        SHUTDOWN_PENDING,
        SHUTDOWN_SENT,
        SHUTDOWN_RECEIVED,
        SHUTDOWN_ACK_SENT
    };

    /**
     * @brief SCTP Path Information for Multi-homing
     */
    struct PathInfo
    {
        Ipv4Address address;        ///< Remote IP Address
        bool active;                ///< Whether path is active
        uint32_t errorCount;        ///< Retransmission error counter
        uint32_t cwnd;              ///< Congestion Window (bytes)
        uint32_t ssthresh;          ///< Slow Start Threshold (bytes)
        uint32_t flightSize;        ///< Data outstanding on this path (bytes)
        Time rto;                   ///< Retransmission Timeout
        EventId hbEvent;            ///< Heartbeat Timer Event
    };

    static TypeId GetTypeId();

    SctpAssociation();
    ~SctpAssociation() override;

    /**
     * @brief Set the owning SctpSocket
     * @param socket The SctpSocket
     */
    void SetSocket(Ptr<SctpSocket> socket);

    /**
     * @brief Get the owning SctpSocket
     * @return The SctpSocket
     */
    Ptr<SctpSocket> GetSocket() const;

    /**
     * @brief Get the association state
     * @return State
     */
    State GetState() const;

    /**
     * @brief Set the association state
     * @param state State
     */
    void SetState(State state);

    /**
     * @brief Set local multi-homing addresses
     * @param addrs Local addresses
     */
    void SetLocalAddresses(const std::vector<Ipv4Address> &addrs);

    /**
     * @brief Get local multi-homing addresses
     * @return Local addresses
     */
    const std::vector<Ipv4Address>& GetLocalAddresses() const;

    /**
     * @brief Set peer multi-homing addresses
     * @param addrs Peer addresses
     */
    void SetPeerAddresses(const std::vector<Ipv4Address> &addrs);

    /**
     * @brief Get peer paths
     * @return Peer paths
     */
    const std::vector<PathInfo>& GetPeerPaths() const;

    /**
     * @brief Set the primary path address
     * @param addr Primary IP address
     */
    void SetPrimaryPath(Ipv4Address addr);

    /**
     * @brief Get primary path address
     * @return Primary address
     */
    Ipv4Address GetPrimaryPath() const;

    /**
     * @brief Set number of outbound streams
     * @param num Outbound streams limit
     */
    void SetNumOutboundStreams(uint16_t num);

    /**
     * @brief Get number of outbound streams
     * @return Outbound streams limit
     */
    uint16_t GetNumOutboundStreams() const;

    /**
     * @brief Set number of inbound streams
     * @param num Inbound streams limit
     */
    void SetNumInboundStreams(uint16_t num);

    /**
     * @brief Get number of inbound streams
     * @return Inbound streams limit
     */
    uint16_t GetNumInboundStreams() const;

    /**
     * @brief Initiate the 4-way handshake
     */
    void Initiate();

    /**
     * @brief Process an incoming packet
     * @param header The SCTP Header and chunks
     * @param fromAddr Sender IP address
     * @param toAddr Destination IP address
     */
    void ProcessPacket(const SctpHeader &header, Ipv4Address fromAddr, Ipv4Address toAddr);

    /**
     * @brief Send user payload over a specific stream
     * @param packet Data payload
     * @param streamId Stream ID
     * @param ppid Payload Protocol ID
     * @param flags Send flags
     * @return Number of bytes written, or -1 on error
     */
    int SendData(Ptr<Packet> packet, uint16_t streamId, uint32_t ppid, uint32_t flags);

    /**
     * @brief Gracefully close association
     */
    void Close();

  private:
    // Helper methods for packet transmission
    void SendPacket(Ptr<SctpHeader> header, Ipv4Address dest);
    Ipv4Address SelectDestinationPath();
    PathInfo* GetPath(Ipv4Address addr);

    // Handshake chunk builders
    void SendInit();
    void SendInitAck(uint32_t peerTag, uint32_t peerTsn, Ipv4Address dest, const std::vector<Ipv4Address>& peerAddrs);
    void SendCookieEcho();
    void SendCookieAck(Ipv4Address dest);
    void SendHeartbeat(Ipv4Address dest);
    void SendHeartbeatAck(const std::vector<uint8_t> &info, Ipv4Address dest);
    void SendSack();
    void SendShutdown(uint32_t cumTsnAck);
    void SendShutdownAck();
    void SendShutdownComplete(bool tBit);

    // Reassembly / Multi-streaming logic
    void HandleDataChunk(Ptr<SctpChunkData> dataChunk);
    void DeliverOrderedData(uint16_t streamId);

    // Timer methods
    void StartInitTimer();
    void StopInitTimer();
    void InitTimeout();

    void StartCookieTimer();
    void StopCookieTimer();
    void CookieTimeout();

    void StartHbTimer(PathInfo &path);
    void StopHbTimer(PathInfo &path);
    void HbTimeout(Ipv4Address addr);

    void StartRtoTimer();
    void StopRtoTimer();
    void RtoTimeout();

    void ProcessSack(Ptr<SctpChunkSack> sackChunk, Ipv4Address from);

    // Member variables
    Ptr<SctpSocket> m_socket;           ///< Owner socket
    State m_state;                      ///< Association state
    uint32_t m_localTag;                ///< Local initiate tag
    uint32_t m_peerTag;                 ///< Peer initiate tag
    uint32_t m_localTsn;                ///< Current local TSN to assign
    uint32_t m_peerTsn;                 ///< Current peer TSN (expected)
    uint32_t m_cumulativeTsnAck;        ///< Last cumulative TSN we acked
    uint32_t m_peerCumTsnAck;            ///< Cumulative TSN peer acked

    std::vector<Ipv4Address> m_localAddresses;  ///< Bound local IPs
    std::vector<PathInfo> m_peerPaths;         ///< Remote IPs and path info
    Ipv4Address m_primaryPath;                 ///< Primary destination IP

    uint16_t m_numOutboundStreams;      ///< Max outbound streams
    uint16_t m_numInboundStreams;       ///< Max inbound streams

    // Timers
    EventId m_initTimer;                ///< T1-init timer event
    EventId m_cookieTimer;              ///< T1-cookie timer event
    EventId m_rtoTimer;                 ///< T3-rtx timer event

    uint32_t m_initAttempts;            ///< Number of INIT attempts
    uint32_t m_cookieAttempts;          ///< Number of COOKIE ECHO attempts

    // Outbound queue entry
    struct TxData
    {
        uint32_t tsn;
        uint16_t streamId;
        uint16_t streamSeqNum;
        uint32_t ppid;
        std::vector<uint8_t> payload;
        Time sentTime;
        Ipv4Address lastDest;
        bool acknowledged;
    };
    std::vector<TxData> m_txQueue;      ///< Outbound queue of unacknowledged DATA chunks

    std::vector<uint16_t> m_outStreamSeq; ///< Outbound stream sequence numbers (SSNs)

    // Reassembly map: streamId -> (SSN -> packet payload)
    std::map<uint16_t, std::map<uint16_t, Ptr<Packet>>> m_inReassembly;
    std::vector<uint16_t> m_inStreamSeq; ///< Inbound expected SSNs

    // Selective acknowledgment tracking
    std::set<uint32_t> m_rxTsns;        ///< TSNs received out-of-order/above Cumulative TSN Ack
    uint32_t m_lastExpectedTsn;         ///< Expected next cumulative TSN

    // Window and Congestion Control
    uint32_t m_peerRwnd;                ///< Peer's advertised receiver window
    uint32_t m_ssthresh;                ///< Congestion control slow-start threshold
    Ipv4Address m_lastRxAddress;               ///< Source address of the last received packet
};

} // namespace ns3

#endif // SCTP_ASSOCIATION_H
