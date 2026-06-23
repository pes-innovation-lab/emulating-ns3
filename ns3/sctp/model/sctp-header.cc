#include "sctp-header.h"
#include "ns3/log.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("SctpHeader");

// CRC32c Castagnoli lookup table-based calculation
static uint32_t crc32c_table[256];
static bool crc32c_table_initialized = false;

static void InitCrc32cTable()
{
    uint32_t poly = 0x82F63B78; // reflected 0x1EDC6F41
    for (uint32_t i = 0; i < 256; i++)
    {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++)
        {
            if (crc & 1)
            {
                crc = (crc >> 1) ^ poly;
            }
            else
            {
                crc >>= 1;
            }
        }
        crc32c_table[i] = crc;
    }
    crc32c_table_initialized = true;
}

uint32_t CalculateCrc32c(const uint8_t *data, size_t length)
{
    if (!crc32c_table_initialized)
    {
        InitCrc32cTable();
    }
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; i++)
    {
        crc = (crc >> 8) ^ crc32c_table[(crc ^ data[i]) & 0xFF];
    }
    return ~crc;
}

// SctpChunk Base Class
SctpChunk::SctpChunk(ChunkType type, uint8_t flags)
    : m_type(type),
      m_flags(flags)
{
}

SctpChunk::~SctpChunk()
{
}

SctpChunk::ChunkType
SctpChunk::GetType() const
{
    return m_type;
}

uint8_t
SctpChunk::GetFlags() const
{
    return m_flags;
}

void
SctpChunk::SetFlags(uint8_t flags)
{
    m_flags = flags;
}

// SctpChunkInit Class
SctpChunkInit::SctpChunkInit()
    : SctpChunk(INIT, 0),
      m_initiateTag(0),
      m_aRwnd(0),
      m_numOutboundStreams(0),
      m_numInboundStreams(0),
      m_initialTsn(0)
{
}

uint32_t
SctpChunkInit::GetSerializedSize() const
{
    return 16 + 8 * m_ipv4Addresses.size();
}

void
SctpChunkInit::Serialize(Buffer::Iterator &start) const
{
    start.WriteHtonU32(m_initiateTag);
    start.WriteHtonU32(m_aRwnd);
    start.WriteHtonU16(m_numOutboundStreams);
    start.WriteHtonU16(m_numInboundStreams);
    start.WriteHtonU32(m_initialTsn);
    for (const auto &addr : m_ipv4Addresses)
    {
        start.WriteHtonU16(5); // IPv4 Address Parameter Type
        start.WriteHtonU16(8); // Parameter Length
        start.WriteHtonU32(addr.Get());
    }
}

uint32_t
SctpChunkInit::Deserialize(Buffer::Iterator &start, uint32_t bodyLength)
{
    m_initiateTag = start.ReadNtohU32();
    m_aRwnd = start.ReadNtohU32();
    m_numOutboundStreams = start.ReadNtohU16();
    m_numInboundStreams = start.ReadNtohU16();
    m_initialTsn = start.ReadNtohU32();

    uint32_t rem = bodyLength - 16;
    m_ipv4Addresses.clear();
    while (rem >= 4)
    {
        uint16_t paramType = start.ReadNtohU16();
        uint16_t paramLen = start.ReadNtohU16();
        if (paramType == 5 && paramLen == 8 && rem >= 8)
        {
            Ipv4Address addr(start.ReadNtohU32());
            m_ipv4Addresses.push_back(addr);
        }
        else
        {
            uint32_t paddedLen = (paramLen + 3) & ~3;
            start.Next(paddedLen - 4); // skip unknown parameter parameter
        }
        rem -= (paramLen + 3) & ~3;
    }
    return bodyLength;
}

void
SctpChunkInit::Print(std::ostream &os) const
{
    os << "INIT [tag=" << m_initiateTag << ", arwnd=" << m_aRwnd
       << ", out=" << m_numOutboundStreams << ", in=" << m_numInboundStreams
       << ", tsn=" << m_initialTsn << ", addrs=" << m_ipv4Addresses.size() << "]";
}

// SctpChunkInitAck Class
SctpChunkInitAck::SctpChunkInitAck()
    : SctpChunk(INIT_ACK, 0),
      m_initiateTag(0),
      m_aRwnd(0),
      m_numOutboundStreams(0),
      m_numInboundStreams(0),
      m_initialTsn(0)
{
}

uint32_t
SctpChunkInitAck::GetSerializedSize() const
{
    uint32_t size = 16 + 8 * m_ipv4Addresses.size();
    if (!m_cookie.empty())
    {
        size += (4 + m_cookie.size() + 3) & ~3;
    }
    return size;
}

void
SctpChunkInitAck::Serialize(Buffer::Iterator &start) const
{
    start.WriteHtonU32(m_initiateTag);
    start.WriteHtonU32(m_aRwnd);
    start.WriteHtonU16(m_numOutboundStreams);
    start.WriteHtonU16(m_numInboundStreams);
    start.WriteHtonU32(m_initialTsn);
    for (const auto &addr : m_ipv4Addresses)
    {
        start.WriteHtonU16(5); // IPv4 Parameter Type
        start.WriteHtonU16(8); // Parameter Length
        start.WriteHtonU32(addr.Get());
    }
    if (!m_cookie.empty())
    {
        start.WriteHtonU16(7); // State Cookie Parameter Type
        start.WriteHtonU16(4 + m_cookie.size());
        start.Write(m_cookie.data(), m_cookie.size());
        uint32_t paddedLen = (4 + m_cookie.size() + 3) & ~3;
        uint32_t padding = paddedLen - (4 + m_cookie.size());
        for (uint32_t i = 0; i < padding; ++i)
        {
            start.WriteU8(0);
        }
    }
}

uint32_t
SctpChunkInitAck::Deserialize(Buffer::Iterator &start, uint32_t bodyLength)
{
    m_initiateTag = start.ReadNtohU32();
    m_aRwnd = start.ReadNtohU32();
    m_numOutboundStreams = start.ReadNtohU16();
    m_numInboundStreams = start.ReadNtohU16();
    m_initialTsn = start.ReadNtohU32();

    uint32_t rem = bodyLength - 16;
    m_ipv4Addresses.clear();
    m_cookie.clear();
    while (rem >= 4)
    {
        uint16_t paramType = start.ReadNtohU16();
        uint16_t paramLen = start.ReadNtohU16();
        uint32_t paddedLen = (paramLen + 3) & ~3;

        if (paramType == 5 && paramLen == 8 && rem >= 8)
        {
            Ipv4Address addr(start.ReadNtohU32());
            m_ipv4Addresses.push_back(addr);
        }
        else if (paramType == 7 && rem >= paramLen)
        {
            m_cookie.resize(paramLen - 4);
            start.Read(m_cookie.data(), paramLen - 4);
            if (paddedLen > paramLen)
            {
                start.Next(paddedLen - paramLen);
            }
        }
        else
        {
            start.Next(paddedLen - 4);
        }
        rem -= paddedLen;
    }
    return bodyLength;
}

void
SctpChunkInitAck::Print(std::ostream &os) const
{
    os << "INIT ACK [tag=" << m_initiateTag << ", arwnd=" << m_aRwnd
       << ", out=" << m_numOutboundStreams << ", in=" << m_numInboundStreams
       << ", tsn=" << m_initialTsn << ", addrs=" << m_ipv4Addresses.size()
       << ", cookie_len=" << m_cookie.size() << "]";
}

// SctpChunkCookieEcho Class
SctpChunkCookieEcho::SctpChunkCookieEcho()
    : SctpChunk(COOKIE_ECHO, 0)
{
}

uint32_t
SctpChunkCookieEcho::GetSerializedSize() const
{
    return m_cookie.size();
}

void
SctpChunkCookieEcho::Serialize(Buffer::Iterator &start) const
{
    start.Write(m_cookie.data(), m_cookie.size());
}

uint32_t
SctpChunkCookieEcho::Deserialize(Buffer::Iterator &start, uint32_t bodyLength)
{
    m_cookie.resize(bodyLength);
    start.Read(m_cookie.data(), bodyLength);
    return bodyLength;
}

void
SctpChunkCookieEcho::Print(std::ostream &os) const
{
    os << "COOKIE ECHO [len=" << m_cookie.size() << "]";
}

// SctpChunkCookieAck Class
SctpChunkCookieAck::SctpChunkCookieAck()
    : SctpChunk(COOKIE_ACK, 0)
{
}

uint32_t
SctpChunkCookieAck::GetSerializedSize() const
{
    return 0;
}

void
SctpChunkCookieAck::Serialize(Buffer::Iterator &start) const
{
}

uint32_t
SctpChunkCookieAck::Deserialize(Buffer::Iterator &start, uint32_t bodyLength)
{
    return 0;
}

void
SctpChunkCookieAck::Print(std::ostream &os) const
{
    os << "COOKIE ACK";
}

// SctpChunkData Class
SctpChunkData::SctpChunkData()
    : SctpChunk(DATA, 0),
      m_tsn(0),
      m_streamId(0),
      m_streamSeqNum(0),
      m_ppid(0)
{
}

uint32_t
SctpChunkData::GetSerializedSize() const
{
    return 12 + m_payload.size();
}

void
SctpChunkData::Serialize(Buffer::Iterator &start) const
{
    start.WriteHtonU32(m_tsn);
    start.WriteHtonU16(m_streamId);
    start.WriteHtonU16(m_streamSeqNum);
    start.WriteHtonU32(m_ppid);
    start.Write(m_payload.data(), m_payload.size());
}

uint32_t
SctpChunkData::Deserialize(Buffer::Iterator &start, uint32_t bodyLength)
{
    m_tsn = start.ReadNtohU32();
    m_streamId = start.ReadNtohU16();
    m_streamSeqNum = start.ReadNtohU16();
    m_ppid = start.ReadNtohU32();
    uint32_t payloadSize = bodyLength - 12;
    m_payload.resize(payloadSize);
    start.Read(m_payload.data(), payloadSize);
    return bodyLength;
}

void
SctpChunkData::Print(std::ostream &os) const
{
    os << "DATA [tsn=" << m_tsn << ", stream=" << m_streamId
       << ", seq=" << m_streamSeqNum << ", ppid=" << m_ppid
       << ", len=" << m_payload.size() << "]";
}

// SctpChunkSack Class
SctpChunkSack::SctpChunkSack()
    : SctpChunk(SACK, 0),
      m_cumulativeTsnAck(0),
      m_aRwnd(0)
{
}

uint32_t
SctpChunkSack::GetSerializedSize() const
{
    return 12 + 4 * m_gapAckBlocks.size() + 4 * m_duplicateTsns.size();
}

void
SctpChunkSack::Serialize(Buffer::Iterator &start) const
{
    start.WriteHtonU32(m_cumulativeTsnAck);
    start.WriteHtonU32(m_aRwnd);
    start.WriteHtonU16(m_gapAckBlocks.size());
    start.WriteHtonU16(m_duplicateTsns.size());
    for (const auto &block : m_gapAckBlocks)
    {
        start.WriteHtonU16(block.first);
        start.WriteHtonU16(block.second);
    }
    for (uint32_t tsn : m_duplicateTsns)
    {
        start.WriteHtonU32(tsn);
    }
}

uint32_t
SctpChunkSack::Deserialize(Buffer::Iterator &start, uint32_t bodyLength)
{
    m_cumulativeTsnAck = start.ReadNtohU32();
    m_aRwnd = start.ReadNtohU32();
    uint16_t numGapBlocks = start.ReadNtohU16();
    uint16_t numDupTsns = start.ReadNtohU16();

    m_gapAckBlocks.clear();
    for (uint16_t i = 0; i < numGapBlocks; ++i)
    {
        uint16_t startOffset = start.ReadNtohU16();
        uint16_t endOffset = start.ReadNtohU16();
        m_gapAckBlocks.push_back(std::make_pair(startOffset, endOffset));
    }

    m_duplicateTsns.clear();
    for (uint16_t i = 0; i < numDupTsns; ++i)
    {
        m_duplicateTsns.push_back(start.ReadNtohU32());
    }
    return bodyLength;
}

void
SctpChunkSack::Print(std::ostream &os) const
{
    os << "SACK [cum_tsn=" << m_cumulativeTsnAck << ", arwnd=" << m_aRwnd
       << ", gaps=" << m_gapAckBlocks.size() << ", dups=" << m_duplicateTsns.size() << "]";
}

// SctpChunkHeartbeat Class
SctpChunkHeartbeat::SctpChunkHeartbeat(ChunkType type)
    : SctpChunk(type, 0)
{
}

uint32_t
SctpChunkHeartbeat::GetSerializedSize() const
{
    return (4 + m_info.size() + 3) & ~3;
}

void
SctpChunkHeartbeat::Serialize(Buffer::Iterator &start) const
{
    start.WriteHtonU16(1); // Heartbeat Info parameter Type
    start.WriteHtonU16(4 + m_info.size());
    start.Write(m_info.data(), m_info.size());
    uint32_t paddedLen = (4 + m_info.size() + 3) & ~3;
    uint32_t padding = paddedLen - (4 + m_info.size());
    for (uint32_t i = 0; i < padding; ++i)
    {
        start.WriteU8(0);
    }
}

uint32_t
SctpChunkHeartbeat::Deserialize(Buffer::Iterator &start, uint32_t bodyLength)
{
    if (bodyLength >= 4)
    {
        uint16_t paramType = start.ReadNtohU16();
        uint16_t paramLen = start.ReadNtohU16();
        uint32_t paddedLen = (paramLen + 3) & ~3;
        if (paramType == 1 && bodyLength >= paramLen)
        {
            m_info.resize(paramLen - 4);
            start.Read(m_info.data(), paramLen - 4);
            if (paddedLen > paramLen)
            {
                start.Next(paddedLen - paramLen);
            }
        }
        else
        {
            start.Next(bodyLength - 4);
        }
    }
    return bodyLength;
}

void
SctpChunkHeartbeat::Print(std::ostream &os) const
{
    os << (m_type == HEARTBEAT ? "HEARTBEAT" : "HEARTBEAT ACK")
       << " [len=" << m_info.size() << "]";
}

// SctpChunkHeartbeatAck Class
SctpChunkHeartbeatAck::SctpChunkHeartbeatAck(ChunkType type)
    : SctpChunkHeartbeat(type)
{
}

// SctpChunkShutdown Class
SctpChunkShutdown::SctpChunkShutdown()
    : SctpChunk(SHUTDOWN, 0),
      m_cumulativeTsnAck(0)
{
}

uint32_t
SctpChunkShutdown::GetSerializedSize() const
{
    return 4;
}

void
SctpChunkShutdown::Serialize(Buffer::Iterator &start) const
{
    start.WriteHtonU32(m_cumulativeTsnAck);
}

uint32_t
SctpChunkShutdown::Deserialize(Buffer::Iterator &start, uint32_t bodyLength)
{
    m_cumulativeTsnAck = start.ReadNtohU32();
    return bodyLength;
}

void
SctpChunkShutdown::Print(std::ostream &os) const
{
    os << "SHUTDOWN [cum_tsn=" << m_cumulativeTsnAck << "]";
}

// SctpChunkShutdownAck Class
SctpChunkShutdownAck::SctpChunkShutdownAck()
    : SctpChunk(SHUTDOWN_ACK, 0)
{
}

uint32_t
SctpChunkShutdownAck::GetSerializedSize() const
{
    return 0;
}

void
SctpChunkShutdownAck::Serialize(Buffer::Iterator &start) const
{
}

uint32_t
SctpChunkShutdownAck::Deserialize(Buffer::Iterator &start, uint32_t bodyLength)
{
    return 0;
}

void
SctpChunkShutdownAck::Print(std::ostream &os) const
{
    os << "SHUTDOWN ACK";
}

// SctpChunkShutdownComplete Class
SctpChunkShutdownComplete::SctpChunkShutdownComplete()
    : SctpChunk(SHUTDOWN_COMPLETE, 0)
{
}

uint32_t
SctpChunkShutdownComplete::GetSerializedSize() const
{
    return 0;
}

void
SctpChunkShutdownComplete::Serialize(Buffer::Iterator &start) const
{
}

uint32_t
SctpChunkShutdownComplete::Deserialize(Buffer::Iterator &start, uint32_t bodyLength)
{
    return 0;
}

void
SctpChunkShutdownComplete::Print(std::ostream &os) const
{
    os << "SHUTDOWN COMPLETE [T-bit=" << (m_flags & 1) << "]";
}

// SctpChunkAbort Class
SctpChunkAbort::SctpChunkAbort()
    : SctpChunk(ABORT, 0)
{
}

uint32_t
SctpChunkAbort::GetSerializedSize() const
{
    return 0;
}

void
SctpChunkAbort::Serialize(Buffer::Iterator &start) const
{
}

uint32_t
SctpChunkAbort::Deserialize(Buffer::Iterator &start, uint32_t bodyLength)
{
    return 0;
}

void
SctpChunkAbort::Print(std::ostream &os) const
{
    os << "ABORT [T-bit=" << (m_flags & 1) << "]";
}

// SctpHeader Class
SctpHeader::SctpHeader()
    : m_sourcePort(0),
      m_destinationPort(0),
      m_verificationTag(0),
      m_checksum(0)
{
}

SctpHeader::~SctpHeader()
{
}

TypeId
SctpHeader::GetTypeId()
{
    static TypeId tid = TypeId("ns3::SctpHeader")
                            .SetParent<Header>()
                            .SetGroupName("Sctp")
                            .AddConstructor<SctpHeader>();
    return tid;
}

TypeId
SctpHeader::GetInstanceTypeId() const
{
    return GetTypeId();
}

void
SctpHeader::Print(std::ostream &os) const
{
    os << "SCTP [src=" << m_sourcePort << ", dst=" << m_destinationPort
       << ", vtag=" << m_verificationTag << ", check=" << m_checksum << "]";
    for (const auto &chunk : m_chunks)
    {
        os << " | ";
        chunk->Print(os);
    }
}

uint32_t
SctpHeader::GetSerializedSize() const
{
    uint32_t size = 12; // Common header
    for (const auto &chunk : m_chunks)
    {
        // Each chunk has 4 bytes header (Type, Flags, Length)
        // plus chunk-specific body size
        uint32_t chunkSize = 4 + chunk->GetSerializedSize();
        // Bundled chunks are padded to a 4-byte boundary
        size += (chunkSize + 3) & ~3;
    }
    return size;
}

void
SctpHeader::Serialize(Buffer::Iterator start) const
{
    Buffer::Iterator it = start;
    it.WriteHtonU16(m_sourcePort);
    it.WriteHtonU16(m_destinationPort);
    it.WriteHtonU32(m_verificationTag);
    it.WriteHtonU32(0); // placeholder checksum

    for (const auto &chunk : m_chunks)
    {
        it.WriteU8(chunk->GetType());
        it.WriteU8(chunk->GetFlags());
        uint32_t bodySize = chunk->GetSerializedSize();
        it.WriteHtonU16(4 + bodySize); // Chunk Length field includes type, flags, length, and body
        chunk->Serialize(it);

        // Pad chunk to a multiple of 4 bytes
        uint32_t paddedSize = (4 + bodySize + 3) & ~3;
        uint32_t padding = paddedSize - (4 + bodySize);
        for (uint32_t i = 0; i < padding; ++i)
        {
            it.WriteU8(0);
        }
    }

    // Now calculate CRC32c over the whole serialized packet
    uint32_t totalSize = GetSerializedSize();
    std::vector<uint8_t> buffer(totalSize);
    Buffer::Iterator readIt = start;
    readIt.Read(buffer.data(), totalSize);

    m_checksum = CalculateCrc32c(buffer.data(), totalSize);

    // Overwrite the checksum field in little-endian order (per RFC 4960)
    Buffer::Iterator writeIt = start;
    writeIt.Next(8);
    writeIt.WriteU8(m_checksum & 0xFF);
    writeIt.WriteU8((m_checksum >> 8) & 0xFF);
    writeIt.WriteU8((m_checksum >> 16) & 0xFF);
    writeIt.WriteU8((m_checksum >> 24) & 0xFF);
}

uint32_t
SctpHeader::Deserialize(Buffer::Iterator start)
{
    Buffer::Iterator it = start;
    m_sourcePort = it.ReadNtohU16();
    m_destinationPort = it.ReadNtohU16();
    m_verificationTag = it.ReadNtohU32();
    // Read checksum in little-endian order
    uint32_t b0 = it.ReadU8();
    uint32_t b1 = it.ReadU8();
    uint32_t b2 = it.ReadU8();
    uint32_t b3 = it.ReadU8();
    m_checksum = b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);

    m_chunks.clear();
    uint32_t bytesRead = 12;

    while (it.GetRemainingSize() >= 4)
    {
        uint8_t type = it.ReadU8();
        uint8_t flags = it.ReadU8();
        uint16_t length = it.ReadNtohU16();

        if (length < 4)
        {
            break; // prevents infinite loop on corrupt chunk length
        }

        Ptr<SctpChunk> chunk = nullptr;
        switch (type)
        {
        case SctpChunk::INIT:
            chunk = Create<SctpChunkInit>();
            break;
        case SctpChunk::INIT_ACK:
            chunk = Create<SctpChunkInitAck>();
            break;
        case SctpChunk::COOKIE_ECHO:
            chunk = Create<SctpChunkCookieEcho>();
            break;
        case SctpChunk::COOKIE_ACK:
            chunk = Create<SctpChunkCookieAck>();
            break;
        case SctpChunk::DATA:
            chunk = Create<SctpChunkData>();
            break;
        case SctpChunk::SACK:
            chunk = Create<SctpChunkSack>();
            break;
        case SctpChunk::HEARTBEAT:
            chunk = Create<SctpChunkHeartbeat>(SctpChunk::HEARTBEAT);
            break;
        case SctpChunk::HEARTBEAT_ACK:
            chunk = Create<SctpChunkHeartbeatAck>(SctpChunk::HEARTBEAT_ACK);
            break;
        case SctpChunk::SHUTDOWN:
            chunk = Create<SctpChunkShutdown>();
            break;
        case SctpChunk::SHUTDOWN_ACK:
            chunk = Create<SctpChunkShutdownAck>();
            break;
        case SctpChunk::SHUTDOWN_COMPLETE:
            chunk = Create<SctpChunkShutdownComplete>();
            break;
        case SctpChunk::ABORT:
            chunk = Create<SctpChunkAbort>();
            break;
        default:
            break; // unknown chunk type
        }

        uint32_t bodyLen = length - 4;
        if (chunk != nullptr)
        {
            chunk->SetFlags(flags);
            chunk->Deserialize(it, bodyLen);
            m_chunks.push_back(chunk);
        }
        else
        {
            it.Next(bodyLen);
        }

        uint32_t paddedSize = (length + 3) & ~3;
        uint32_t padding = paddedSize - length;
        if (padding > 0 && it.GetRemainingSize() >= padding)
        {
            it.Next(padding);
        }
        bytesRead += paddedSize;
    }
    return bytesRead;
}

void
SctpHeader::SetSourcePort(uint16_t port)
{
    m_sourcePort = port;
}

uint16_t
SctpHeader::GetSourcePort() const
{
    return m_sourcePort;
}

void
SctpHeader::SetDestinationPort(uint16_t port)
{
    m_destinationPort = port;
}

uint16_t
SctpHeader::GetDestinationPort() const
{
    return m_destinationPort;
}

void
SctpHeader::SetVerificationTag(uint32_t tag)
{
    m_verificationTag = tag;
}

uint32_t
SctpHeader::GetVerificationTag() const
{
    return m_verificationTag;
}

void
SctpHeader::SetChecksum(uint32_t checksum)
{
    m_checksum = checksum;
}

uint32_t
SctpHeader::GetChecksum() const
{
    return m_checksum;
}

void
SctpHeader::AddChunk(Ptr<SctpChunk> chunk)
{
    m_chunks.push_back(chunk);
}

const std::vector<Ptr<SctpChunk>>&
SctpHeader::GetChunks() const
{
    return m_chunks;
}

} // namespace ns3
