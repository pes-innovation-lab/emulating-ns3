#ifndef SCTP_HEADER_H
#define SCTP_HEADER_H

#include "ns3/header.h"
#include "ns3/simple-ref-count.h"
#include "ns3/ipv4-address.h"
#include "ns3/packet.h"
#include <vector>
#include <iostream>
#include <utility>

namespace ns3
{

/**
 * @ingroup sctp
 * @brief Base class for SCTP Chunks
 */
class SctpChunk : public SimpleRefCount<SctpChunk>
{
  public:
    /**
     * @brief SCTP Chunk Types
     */
    enum ChunkType
    {
        DATA = 0,
        INIT = 1,
        INIT_ACK = 2,
        SACK = 3,
        HEARTBEAT = 4,
        HEARTBEAT_ACK = 5,
        ABORT = 6,
        SHUTDOWN = 7,
        SHUTDOWN_ACK = 8,
        ERROR_CHUNK = 9,
        COOKIE_ECHO = 10,
        COOKIE_ACK = 11,
        ECNE = 12,
        CWR = 13,
        SHUTDOWN_COMPLETE = 14
    };

    /**
     * @brief Constructor
     * @param type Chunk type
     * @param flags Chunk flags
     */
    SctpChunk(ChunkType type, uint8_t flags);

    /**
     * @brief Destructor
     */
    virtual ~SctpChunk();

    /**
     * @brief Get the type of chunk
     * @return The chunk type
     */
    ChunkType GetType() const;

    /**
     * @brief Get the chunk flags
     * @return The chunk flags
     */
    uint8_t GetFlags() const;

    /**
     * @brief Set the chunk flags
     * @param flags The chunk flags
     */
    void SetFlags(uint8_t flags);

    /**
     * @brief Get serialized size of the chunk (excluding padding)
     * @return Serialized size of chunk in bytes
     */
    virtual uint32_t GetSerializedSize() const = 0;

    /**
     * @brief Serialize the chunk body
     * @param start Iterator to write chunk body to
     */
    virtual void Serialize(Buffer::Iterator &start) const = 0;

    /**
     * @brief Deserialize the chunk body
     * @param start Iterator to read chunk body from
     * @param bodyLength Length of the chunk body in bytes
     * @return The number of bytes read
     */
    virtual uint32_t Deserialize(Buffer::Iterator &start, uint32_t bodyLength) = 0;

    /**
     * @brief Print the chunk to ostream
     * @param os Output stream
     */
    virtual void Print(std::ostream &os) const = 0;

  protected:
    ChunkType m_type;   ///< Chunk type
    uint8_t m_flags;    ///< Chunk flags
};

/**
 * @ingroup sctp
 * @brief SCTP INIT Chunk
 */
class SctpChunkInit : public SctpChunk
{
  public:
    SctpChunkInit();

    /**
     * @brief Get serialized size of the chunk
     * @return Serialized size
     */
    uint32_t GetSerializedSize() const override;

    /**
     * @brief Serialize the chunk body
     * @param start Iterator
     */
    void Serialize(Buffer::Iterator &start) const override;

    /**
     * @brief Deserialize the chunk body
     * @param start Iterator
     * @param bodyLength Body length
     * @return Bytes read
     */
    uint32_t Deserialize(Buffer::Iterator &start, uint32_t bodyLength) override;

    /**
     * @brief Print chunk
     * @param os Stream
     */
    void Print(std::ostream &os) const override;

    uint32_t m_initiateTag;                     ///< Initiate Tag
    uint32_t m_aRwnd;                           ///< Advertised Receiver Window Credit
    uint16_t m_numOutboundStreams;              ///< Number of Outbound Streams
    uint16_t m_numInboundStreams;               ///< Number of Inbound Streams
    uint32_t m_initialTsn;                      ///< Initial TSN
    std::vector<Ipv4Address> m_ipv4Addresses;   ///< List of IPv4 local addresses
};

/**
 * @ingroup sctp
 * @brief SCTP INIT ACK Chunk
 */
class SctpChunkInitAck : public SctpChunk
{
  public:
    SctpChunkInitAck();

    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator &start) const override;
    uint32_t Deserialize(Buffer::Iterator &start, uint32_t bodyLength) override;
    void Print(std::ostream &os) const override;

    uint32_t m_initiateTag;                     ///< Initiate Tag
    uint32_t m_aRwnd;                           ///< Advertised Receiver Window Credit
    uint16_t m_numOutboundStreams;              ///< Number of Outbound Streams
    uint16_t m_numInboundStreams;               ///< Number of Inbound Streams
    uint32_t m_initialTsn;                      ///< Initial TSN
    std::vector<Ipv4Address> m_ipv4Addresses;   ///< List of IPv4 local addresses
    std::vector<uint8_t> m_cookie;              ///< State Cookie Parameter
};

/**
 * @ingroup sctp
 * @brief SCTP COOKIE ECHO Chunk
 */
class SctpChunkCookieEcho : public SctpChunk
{
  public:
    SctpChunkCookieEcho();

    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator &start) const override;
    uint32_t Deserialize(Buffer::Iterator &start, uint32_t bodyLength) override;
    void Print(std::ostream &os) const override;

    std::vector<uint8_t> m_cookie;              ///< State Cookie
};

/**
 * @ingroup sctp
 * @brief SCTP COOKIE ACK Chunk
 */
class SctpChunkCookieAck : public SctpChunk
{
  public:
    SctpChunkCookieAck();

    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator &start) const override;
    uint32_t Deserialize(Buffer::Iterator &start, uint32_t bodyLength) override;
    void Print(std::ostream &os) const override;
};

/**
 * @ingroup sctp
 * @brief SCTP DATA Chunk
 */
class SctpChunkData : public SctpChunk
{
  public:
    SctpChunkData();

    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator &start) const override;
    uint32_t Deserialize(Buffer::Iterator &start, uint32_t bodyLength) override;
    void Print(std::ostream &os) const override;

    uint32_t m_tsn;                             ///< TSN
    uint16_t m_streamId;                        ///< Stream ID
    uint16_t m_streamSeqNum;                    ///< Stream Sequence Number
    uint32_t m_ppid;                            ///< Payload Protocol Identifier
    std::vector<uint8_t> m_payload;             ///< Payload user data
};

/**
 * @ingroup sctp
 * @brief SCTP SACK Chunk
 */
class SctpChunkSack : public SctpChunk
{
  public:
    SctpChunkSack();

    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator &start) const override;
    uint32_t Deserialize(Buffer::Iterator &start, uint32_t bodyLength) override;
    void Print(std::ostream &os) const override;

    uint32_t m_cumulativeTsnAck;                                ///< Cumulative TSN Ack
    uint32_t m_aRwnd;                                           ///< Advertised Receiver Window Credit
    std::vector<std::pair<uint16_t, uint16_t>> m_gapAckBlocks;  ///< Gap Ack Blocks (offsets)
    std::vector<uint32_t> m_duplicateTsns;                      ///< Duplicate TSNs
};

/**
 * @ingroup sctp
 * @brief SCTP HEARTBEAT Chunk
 */
class SctpChunkHeartbeat : public SctpChunk
{
  public:
    SctpChunkHeartbeat(ChunkType type = HEARTBEAT);

    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator &start) const override;
    uint32_t Deserialize(Buffer::Iterator &start, uint32_t bodyLength) override;
    void Print(std::ostream &os) const override;

    std::vector<uint8_t> m_info;                                ///< Heartbeat Information
};

/**
 * @ingroup sctp
 * @brief SCTP HEARTBEAT ACK Chunk
 */
class SctpChunkHeartbeatAck : public SctpChunkHeartbeat
{
  public:
    SctpChunkHeartbeatAck(ChunkType type = HEARTBEAT_ACK);
};

/**
 * @ingroup sctp
 * @brief SCTP SHUTDOWN Chunk
 */
class SctpChunkShutdown : public SctpChunk
{
  public:
    SctpChunkShutdown();

    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator &start) const override;
    uint32_t Deserialize(Buffer::Iterator &start, uint32_t bodyLength) override;
    void Print(std::ostream &os) const override;

    uint32_t m_cumulativeTsnAck;                                ///< Cumulative TSN Ack
};

/**
 * @ingroup sctp
 * @brief SCTP SHUTDOWN ACK Chunk
 */
class SctpChunkShutdownAck : public SctpChunk
{
  public:
    SctpChunkShutdownAck();

    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator &start) const override;
    uint32_t Deserialize(Buffer::Iterator &start, uint32_t bodyLength) override;
    void Print(std::ostream &os) const override;
};

/**
 * @ingroup sctp
 * @brief SCTP SHUTDOWN COMPLETE Chunk
 */
class SctpChunkShutdownComplete : public SctpChunk
{
  public:
    SctpChunkShutdownComplete();

    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator &start) const override;
    uint32_t Deserialize(Buffer::Iterator &start, uint32_t bodyLength) override;
    void Print(std::ostream &os) const override;
};

/**
 * @ingroup sctp
 * @brief SCTP ABORT Chunk
 */
class SctpChunkAbort : public SctpChunk
{
  public:
    SctpChunkAbort();

    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator &start) const override;
    uint32_t Deserialize(Buffer::Iterator &start, uint32_t bodyLength) override;
    void Print(std::ostream &os) const override;
};


/**
 * @ingroup sctp
 * @brief Common SCTP Header (RFC 4960)
 */
class SctpHeader : public Header, public SimpleRefCount<SctpHeader>
{
  public:
    SctpHeader();
    ~SctpHeader() override;

    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;
    void Print(std::ostream &os) const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;

    void SetSourcePort(uint16_t port);
    uint16_t GetSourcePort() const;

    void SetDestinationPort(uint16_t port);
    uint16_t GetDestinationPort() const;

    void SetVerificationTag(uint32_t tag);
    uint32_t GetVerificationTag() const;

    void SetChecksum(uint32_t checksum);
    uint32_t GetChecksum() const;

    void AddChunk(Ptr<SctpChunk> chunk);
    const std::vector<Ptr<SctpChunk>>& GetChunks() const;

  private:
    uint16_t m_sourcePort;              ///< Source Port
    uint16_t m_destinationPort;         ///< Destination Port
    uint32_t m_verificationTag;         ///< Verification Tag
    mutable uint32_t m_checksum;        ///< Checksum (CRC32c)
    std::vector<Ptr<SctpChunk>> m_chunks; ///< List of chunks bundled in packet
};

/**
 * @brief Compute the CRC32c (Castagnoli) checksum
 * @param data Byte array pointer
 * @param length Byte length
 * @return Computed 32-bit CRC32c value
 */
uint32_t CalculateCrc32c(const uint8_t *data, size_t length);

} // namespace ns3

#endif // SCTP_HEADER_H
