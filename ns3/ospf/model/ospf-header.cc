#include "ospf-header.h"
#include "ns3/log.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("OspfHeader");

// --- OspfHeader ---
OspfHeader::OspfHeader()
  : m_version(2),
    m_type(0),
    m_packetLength(0),
    m_routerId(Ipv4Address::GetAny()),
    m_areaId(Ipv4Address::GetAny()),
    m_checksum(0),
    m_auType(0)
{
  std::memset(m_authentication, 0, 8);
}

OspfHeader::~OspfHeader()
{}

TypeId
OspfHeader::GetTypeId(void)
{
  static TypeId tid = TypeId("ns3::OspfHeader")
    .SetParent<Header>()
    .SetGroupName("Ospf")
    .AddConstructor<OspfHeader>()
  ;
  return tid;
}

TypeId
OspfHeader::GetInstanceTypeId(void) const
{
  return GetTypeId();
}

uint32_t
OspfHeader::GetSerializedSize(void) const
{
  return 24;
}

void
OspfHeader::Serialize(Buffer::Iterator start) const
{
  Buffer::Iterator originalStart = start;

  start.WriteU8(m_version);
  start.WriteU8(m_type);
  start.WriteHtonU16(m_packetLength);
  start.WriteHtonU32(m_routerId.Get());
  start.WriteHtonU32(m_areaId.Get());
  start.WriteHtonU16(0); // Write checksum field as 0 temporarily
  start.WriteHtonU16(m_auType);
  start.Write(m_authentication, 8);

  // Calculate the checksum over the entire OSPF packet
  Buffer::Iterator i = originalStart;
  uint16_t checksum = i.CalculateIpChecksum(m_packetLength);

  // Write calculated checksum back to offset 12 in network byte order
  i = originalStart;
  i.Next(12);
  i.WriteU8(checksum & 0xFF);
  i.WriteU8(checksum >> 8);
}

uint32_t
OspfHeader::Deserialize(Buffer::Iterator start)
{
  m_version = start.ReadU8();
  m_type = start.ReadU8();
  m_packetLength = start.ReadNtohU16();
  m_routerId.Set(start.ReadNtohU32());
  m_areaId.Set(start.ReadNtohU32());
  m_checksum = start.ReadNtohU16();
  m_auType = start.ReadNtohU16();
  start.Read(m_authentication, 8);
  return 24;
}

void
OspfHeader::Print(std::ostream &os) const
{
  os << "OSPF Header (Ver: " << (uint16_t)m_version
     << ", Type: " << (uint16_t)m_type
     << ", Len: " << m_packetLength
     << ", RtrID: " << m_routerId
     << ", Area: " << m_areaId << ")";
}

NS_OBJECT_ENSURE_REGISTERED(OspfHeader);


// --- OspfHelloHeader ---
OspfHelloHeader::OspfHelloHeader()
  : m_networkMask(Ipv4Address::GetAny()),
    m_helloInterval(10),
    m_options(2),
    m_rtrPriority(1),
    m_routerDeadInterval(40),
    m_designatedRouter(Ipv4Address::GetAny()),
    m_backupDesignatedRouter(Ipv4Address::GetAny())
{}

OspfHelloHeader::~OspfHelloHeader()
{}

TypeId
OspfHelloHeader::GetTypeId(void)
{
  static TypeId tid = TypeId("ns3::OspfHelloHeader")
    .SetParent<Header>()
    .SetGroupName("Ospf")
    .AddConstructor<OspfHelloHeader>()
  ;
  return tid;
}

TypeId
OspfHelloHeader::GetInstanceTypeId(void) const
{
  return GetTypeId();
}

uint32_t
OspfHelloHeader::GetSerializedSize(void) const
{
  return 20 + 4 * m_neighbors.size();
}

void
OspfHelloHeader::Serialize(Buffer::Iterator start) const
{
  start.WriteHtonU32(m_networkMask.Get());
  start.WriteHtonU16(m_helloInterval);
  start.WriteU8(m_options);
  start.WriteU8(m_rtrPriority);
  start.WriteHtonU32(m_routerDeadInterval);
  start.WriteHtonU32(m_designatedRouter.Get());
  start.WriteHtonU32(m_backupDesignatedRouter.Get());
  for (const auto& neighbor : m_neighbors)
    {
      start.WriteHtonU32(neighbor.Get());
    }
}

uint32_t
OspfHelloHeader::Deserialize(Buffer::Iterator start)
{
  m_networkMask.Set(start.ReadNtohU32());
  m_helloInterval = start.ReadNtohU16();
  m_options = start.ReadU8();
  m_rtrPriority = start.ReadU8();
  m_routerDeadInterval = start.ReadNtohU32();
  m_designatedRouter.Set(start.ReadNtohU32());
  m_backupDesignatedRouter.Set(start.ReadNtohU32());

  // Note: the packet length in OspfHeader determines how many neighbors we read.
  // However, Header::Deserialize has no context of parent size directly unless we determine it.
  // So we read until buffer iterator is exhausted or we can structure the parse.
  // Wait, ns-3 deserialization can be read until end. We can read remaining bytes.
  m_neighbors.clear();
  while (start.GetRemainingSize() >= 4)
    {
      Ipv4Address neighbor;
      neighbor.Set(start.ReadNtohU32());
      m_neighbors.push_back(neighbor);
    }
  return 20 + 4 * m_neighbors.size();
}

void
OspfHelloHeader::Print(std::ostream &os) const
{
  os << "OSPF Hello (Mask: " << m_networkMask
     << ", HelloInt: " << m_helloInterval
     << ", Pri: " << (uint16_t)m_rtrPriority
     << ", DeadInt: " << m_routerDeadInterval
     << ", DR: " << m_designatedRouter
     << ", BDR: " << m_backupDesignatedRouter
     << ", Neighbors Count: " << m_neighbors.size() << ")";
}

NS_OBJECT_ENSURE_REGISTERED(OspfHelloHeader);


// --- OspfDatabaseDescriptionHeader ---
OspfDatabaseDescriptionHeader::OspfDatabaseDescriptionHeader()
  : m_interfaceMtu(1500),
    m_options(2),
    m_flags(0),
    m_ddSequenceNumber(0)
{}

OspfDatabaseDescriptionHeader::~OspfDatabaseDescriptionHeader()
{}

TypeId
OspfDatabaseDescriptionHeader::GetTypeId(void)
{
  static TypeId tid = TypeId("ns3::OspfDatabaseDescriptionHeader")
    .SetParent<Header>()
    .SetGroupName("Ospf")
    .AddConstructor<OspfDatabaseDescriptionHeader>()
  ;
  return tid;
}

TypeId
OspfDatabaseDescriptionHeader::GetInstanceTypeId(void) const
{
  return GetTypeId();
}

uint32_t
OspfDatabaseDescriptionHeader::GetSerializedSize(void) const
{
  return 8 + 20 * m_lsaHeaders.size();
}

void
OspfDatabaseDescriptionHeader::Serialize(Buffer::Iterator start) const
{
  start.WriteHtonU16(m_interfaceMtu);
  start.WriteU8(m_options);
  start.WriteU8(m_flags);
  start.WriteHtonU32(m_ddSequenceNumber);
  for (const auto& lsaHeader : m_lsaHeaders)
    {
      lsaHeader.Serialize(start);
    }
}

uint32_t
OspfDatabaseDescriptionHeader::Deserialize(Buffer::Iterator start)
{
  m_interfaceMtu = start.ReadNtohU16();
  m_options = start.ReadU8();
  m_flags = start.ReadU8();
  m_ddSequenceNumber = start.ReadNtohU32();

  m_lsaHeaders.clear();
  while (start.GetRemainingSize() >= 20)
    {
      OspfLsaHeader lsaHeader;
      lsaHeader.Deserialize(start);
      m_lsaHeaders.push_back(lsaHeader);
    }
  return 8 + 20 * m_lsaHeaders.size();
}

void
OspfDatabaseDescriptionHeader::Print(std::ostream &os) const
{
  os << "OSPF DD (MTU: " << m_interfaceMtu
     << ", Flags: " << (uint16_t)m_flags
     << ", Seq: " << m_ddSequenceNumber
     << ", Summaries Count: " << m_lsaHeaders.size() << ")";
}

NS_OBJECT_ENSURE_REGISTERED(OspfDatabaseDescriptionHeader);


// --- OspfLinkStateRequestHeader ---
OspfLinkStateRequestHeader::OspfLinkStateRequestHeader()
{}

OspfLinkStateRequestHeader::~OspfLinkStateRequestHeader()
{}

TypeId
OspfLinkStateRequestHeader::GetTypeId(void)
{
  static TypeId tid = TypeId("ns3::OspfLinkStateRequestHeader")
    .SetParent<Header>()
    .SetGroupName("Ospf")
    .AddConstructor<OspfLinkStateRequestHeader>()
  ;
  return tid;
}

TypeId
OspfLinkStateRequestHeader::GetInstanceTypeId(void) const
{
  return GetTypeId();
}

uint32_t
OspfLinkStateRequestHeader::GetSerializedSize(void) const
{
  return 12 * m_requests.size();
}

void
OspfLinkStateRequestHeader::Serialize(Buffer::Iterator start) const
{
  for (const auto& request : m_requests)
    {
      start.WriteHtonU32(request.m_lsType);
      start.WriteHtonU32(request.m_linkStateId.Get());
      start.WriteHtonU32(request.m_advertisingRouter.Get());
    }
}

uint32_t
OspfLinkStateRequestHeader::Deserialize(Buffer::Iterator start)
{
  m_requests.clear();
  while (start.GetRemainingSize() >= 12)
    {
      RequestRecord request;
      request.m_lsType = start.ReadNtohU32();
      request.m_linkStateId.Set(start.ReadNtohU32());
      request.m_advertisingRouter.Set(start.ReadNtohU32());
      m_requests.push_back(request);
    }
  return 12 * m_requests.size();
}

void
OspfLinkStateRequestHeader::Print(std::ostream &os) const
{
  os << "OSPF LSR (Requests Count: " << m_requests.size() << ")";
}

NS_OBJECT_ENSURE_REGISTERED(OspfLinkStateRequestHeader);


// --- OspfLinkStateUpdateHeader ---
OspfLinkStateUpdateHeader::OspfLinkStateUpdateHeader()
{}

OspfLinkStateUpdateHeader::~OspfLinkStateUpdateHeader()
{}

TypeId
OspfLinkStateUpdateHeader::GetTypeId(void)
{
  static TypeId tid = TypeId("ns3::OspfLinkStateUpdateHeader")
    .SetParent<Header>()
    .SetGroupName("Ospf")
    .AddConstructor<OspfLinkStateUpdateHeader>()
  ;
  return tid;
}

TypeId
OspfLinkStateUpdateHeader::GetInstanceTypeId(void) const
{
  return GetTypeId();
}

uint32_t
OspfLinkStateUpdateHeader::GetSerializedSize(void) const
{
  uint32_t totalSize = 4; // Number of LSAs field
  for (const auto& lsa : m_routerLsas)
    {
      totalSize += 20 + 4 + 12 * lsa.m_links.size();
    }
  for (const auto& lsa : m_networkLsas)
    {
      totalSize += 20 + 4 + 4 * lsa.m_attachedRouters.size();
    }
  return totalSize;
}

void
OspfLinkStateUpdateHeader::Serialize(Buffer::Iterator start) const
{
  uint32_t numLsas = m_routerLsas.size() + m_networkLsas.size();
  start.WriteHtonU32(numLsas);
  for (const auto& lsa : m_routerLsas)
    {
      lsa.Serialize(start);
    }
  for (const auto& lsa : m_networkLsas)
    {
      lsa.Serialize(start);
    }
}

uint32_t
OspfLinkStateUpdateHeader::Deserialize(Buffer::Iterator start)
{
  uint32_t numLsas = start.ReadNtohU32();
  m_routerLsas.clear();
  m_networkLsas.clear();

  for (uint32_t i = 0; i < numLsas; ++i)
    {
      if (start.GetRemainingSize() < 20)
        {
          break; // Avoid crash on malformed update
        }
      
      // Peek at type: type field is at offset 3 of the 20-byte LSA header
      // We can create a copy of the iterator to read without moving the main iterator
      Buffer::Iterator peekIt = start;
      peekIt.Next(3); // skip age(2) and options(1)
      uint8_t type = peekIt.ReadU8();

      if (type == 1)
        {
          OspfRouterLsa rLsa;
          rLsa.Deserialize(start);
          m_routerLsas.push_back(rLsa);
        }
      else if (type == 2)
        {
          OspfNetworkLsa nLsa;
          nLsa.Deserialize(start);
          m_networkLsas.push_back(nLsa);
        }
      else
        {
          // Unsupported LSA type inside update: read its length and skip it to prevent crash
          Buffer::Iterator skipIt = start;
          skipIt.Next(18); // skip to length field (offset 18)
          uint16_t len = skipIt.ReadNtohU16();
          start.Next(len);
        }
    }

  return GetSerializedSize();
}

void
OspfLinkStateUpdateHeader::Print(std::ostream &os) const
{
  os << "OSPF LSU (RouterLSAs: " << m_routerLsas.size()
     << ", NetworkLSAs: " << m_networkLsas.size() << ")";
}

NS_OBJECT_ENSURE_REGISTERED(OspfLinkStateUpdateHeader);


// --- OspfLinkStateAcknowledgmentHeader ---
OspfLinkStateAcknowledgmentHeader::OspfLinkStateAcknowledgmentHeader()
{}

OspfLinkStateAcknowledgmentHeader::~OspfLinkStateAcknowledgmentHeader()
{}

TypeId
OspfLinkStateAcknowledgmentHeader::GetTypeId(void)
{
  static TypeId tid = TypeId("ns3::OspfLinkStateAcknowledgmentHeader")
    .SetParent<Header>()
    .SetGroupName("Ospf")
    .AddConstructor<OspfLinkStateAcknowledgmentHeader>()
  ;
  return tid;
}

TypeId
OspfLinkStateAcknowledgmentHeader::GetInstanceTypeId(void) const
{
  return GetTypeId();
}

uint32_t
OspfLinkStateAcknowledgmentHeader::GetSerializedSize(void) const
{
  return 20 * m_lsaHeaders.size();
}

void
OspfLinkStateAcknowledgmentHeader::Serialize(Buffer::Iterator start) const
{
  for (const auto& lsaHeader : m_lsaHeaders)
    {
      lsaHeader.Serialize(start);
    }
}

uint32_t
OspfLinkStateAcknowledgmentHeader::Deserialize(Buffer::Iterator start)
{
  m_lsaHeaders.clear();
  while (start.GetRemainingSize() >= 20)
    {
      OspfLsaHeader lsaHeader;
      lsaHeader.Deserialize(start);
      m_lsaHeaders.push_back(lsaHeader);
    }
  return 20 * m_lsaHeaders.size();
}

void
OspfLinkStateAcknowledgmentHeader::Print(std::ostream &os) const
{
  os << "OSPF LSAck (Acks Count: " << m_lsaHeaders.size() << ")";
}

NS_OBJECT_ENSURE_REGISTERED(OspfLinkStateAcknowledgmentHeader);

} // namespace ns3
