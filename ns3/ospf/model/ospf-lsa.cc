#include "ospf-lsa.h"

namespace ns3 {

// --- OspfLsaHeader ---
OspfLsaHeader::OspfLsaHeader()
  : m_lsAge(0),
    m_options(0),
    m_lsType(0),
    m_linkStateId(Ipv4Address::GetAny()),
    m_advertisingRouter(Ipv4Address::GetAny()),
    m_lsSequenceNumber(0),
    m_lsChecksum(0),
    m_length(20)
{}

void
OspfLsaHeader::Serialize(Buffer::Iterator& start) const
{
  start.WriteHtonU16(m_lsAge);
  start.WriteU8(m_options);
  start.WriteU8(m_lsType);
  start.WriteHtonU32(m_linkStateId.Get());
  start.WriteHtonU32(m_advertisingRouter.Get());
  start.WriteHtonU32(m_lsSequenceNumber);
  start.WriteHtonU16(m_lsChecksum);
  start.WriteHtonU16(m_length);
}

void
OspfLsaHeader::Deserialize(Buffer::Iterator& start)
{
  m_lsAge = start.ReadNtohU16();
  m_options = start.ReadU8();
  m_lsType = start.ReadU8();
  m_linkStateId.Set(start.ReadNtohU32());
  m_advertisingRouter.Set(start.ReadNtohU32());
  m_lsSequenceNumber = start.ReadNtohU32();
  m_lsChecksum = start.ReadNtohU16();
  m_length = start.ReadNtohU16();
}

void
OspfLsaHeader::Print(std::ostream &os) const
{
  os << "Age: " << m_lsAge
     << ", Options: " << (uint16_t)m_options
     << ", Type: " << (uint16_t)m_lsType
     << ", LSID: " << m_linkStateId
     << ", AdvRtr: " << m_advertisingRouter
     << ", Seq: 0x" << std::hex << m_lsSequenceNumber << std::dec
     << ", Length: " << m_length;
}

// --- OspfLinkRecord ---
OspfLinkRecord::OspfLinkRecord()
  : m_linkId(Ipv4Address::GetAny()),
    m_linkData(Ipv4Address::GetAny()),
    m_type(0),
    m_numTos(0),
    m_metric(0)
{}

void
OspfLinkRecord::Serialize(Buffer::Iterator& start) const
{
  start.WriteHtonU32(m_linkId.Get());
  start.WriteHtonU32(m_linkData.Get());
  start.WriteU8(m_type);
  start.WriteU8(m_numTos);
  start.WriteHtonU16(m_metric);
}

void
OspfLinkRecord::Deserialize(Buffer::Iterator& start)
{
  m_linkId.Set(start.ReadNtohU32());
  m_linkData.Set(start.ReadNtohU32());
  m_type = start.ReadU8();
  m_numTos = start.ReadU8();
  m_metric = start.ReadNtohU16();
}

// Helper for calculating OSPF Fletcher-16 checksum on LSA data
static uint16_t
CalculateLsaChecksum (uint8_t *buffer, int length)
{
  int checksumOffset = 14; // offset 16 in LSA header, excluding m_lsAge (2 bytes)
  buffer[checksumOffset] = 0;
  buffer[checksumOffset + 1] = 0;

  int32_t c0 = 0;
  int32_t c1 = 0;

  for (int i = 0; i < length; i++)
    {
      c0 = (c0 + buffer[i]) % 255;
      c1 = (c1 + c0) % 255;
    }

  int32_t x = ((length - checksumOffset - 1) * c0 - c1) % 255;
  if (x <= 0)
    {
      x += 255;
    }
  int32_t y = (510 - c0 - x) % 255;
  if (y <= 0)
    {
      y += 255;
    }

  return (x << 8) | y;
}

// --- OspfRouterLsa ---
OspfRouterLsa::OspfRouterLsa()
  : m_flags(0),
    m_zero(0)
{
  m_header.m_lsType = 1; // Router LSA
}

void
OspfRouterLsa::Serialize(Buffer::Iterator& start) const
{
  Buffer::Iterator origStart = start;

  // Make sure header length is correct
  OspfRouterLsa* mutableThis = const_cast<OspfRouterLsa*>(this);
  mutableThis->m_header.m_length = 20 + 4 + 12 * m_links.size();
  mutableThis->m_header.m_lsChecksum = 0;
  m_header.Serialize(start);

  start.WriteU8(m_flags);
  start.WriteU8(m_zero);
  start.WriteHtonU16(m_links.size());
  for (const auto& link : m_links)
    {
      link.Serialize(start);
    }

  // Calculate checksum over LSA data (excluding the 2-byte LS Age)
  Buffer::Iterator dataIt = origStart;
  dataIt.Next(2);

  uint32_t checksumLength = m_header.m_length - 2;
  std::vector<uint8_t> buffer(checksumLength);
  dataIt.Read(buffer.data(), checksumLength);

  uint16_t checksum = CalculateLsaChecksum(buffer.data(), checksumLength);

  // Write checksum back to offset 16 in the LSA header
  Buffer::Iterator writeIt = origStart;
  writeIt.Next(16);
  writeIt.WriteU8(checksum >> 8);
  writeIt.WriteU8(checksum & 0xFF);
}

void
OspfRouterLsa::Deserialize(Buffer::Iterator& start)
{
  m_header.Deserialize(start);
  m_flags = start.ReadU8();
  m_zero = start.ReadU8();
  uint16_t numLinks = start.ReadNtohU16();
  m_links.clear();
  for (uint16_t i = 0; i < numLinks; ++i)
    {
      OspfLinkRecord link;
      link.Deserialize(start);
      m_links.push_back(link);
    }
}

void
OspfRouterLsa::Print(std::ostream &os) const
{
  m_header.Print(os);
  os << ", Flags: " << (uint16_t)m_flags
     << ", Links Count: " << m_links.size();
}

// --- OspfNetworkLsa ---
OspfNetworkLsa::OspfNetworkLsa()
  : m_networkMask(Ipv4Address::GetAny())
{
  m_header.m_lsType = 2; // Network LSA
}

void
OspfNetworkLsa::Serialize(Buffer::Iterator& start) const
{
  Buffer::Iterator origStart = start;

  // Make sure header length is correct
  OspfNetworkLsa* mutableThis = const_cast<OspfNetworkLsa*>(this);
  mutableThis->m_header.m_length = 20 + 4 + 4 * m_attachedRouters.size();
  mutableThis->m_header.m_lsChecksum = 0;
  m_header.Serialize(start);

  start.WriteHtonU32(m_networkMask.Get());
  for (const auto& rtr : m_attachedRouters)
    {
      start.WriteHtonU32(rtr.Get());
    }

  // Calculate checksum over LSA data (excluding the 2-byte LS Age)
  Buffer::Iterator dataIt = origStart;
  dataIt.Next(2);

  uint32_t checksumLength = m_header.m_length - 2;
  std::vector<uint8_t> buffer(checksumLength);
  dataIt.Read(buffer.data(), checksumLength);

  uint16_t checksum = CalculateLsaChecksum(buffer.data(), checksumLength);

  // Write checksum back to offset 16 in the LSA header
  Buffer::Iterator writeIt = origStart;
  writeIt.Next(16);
  writeIt.WriteU8(checksum >> 8);
  writeIt.WriteU8(checksum & 0xFF);
}

void
OspfNetworkLsa::Deserialize(Buffer::Iterator& start)
{
  m_header.Deserialize(start);
  m_networkMask.Set(start.ReadNtohU32());
  uint16_t numAttached = (m_header.m_length - 24) / 4;
  m_attachedRouters.clear();
  for (uint16_t i = 0; i < numAttached; ++i)
    {
      Ipv4Address rtr;
      rtr.Set(start.ReadNtohU32());
      m_attachedRouters.push_back(rtr);
    }
}

void
OspfNetworkLsa::Print(std::ostream &os) const
{
  m_header.Print(os);
  os << ", Mask: " << m_networkMask
     << ", Attached Routers Count: " << m_attachedRouters.size();
}

} // namespace ns3
