#ifndef OSPF_HEADER_H
#define OSPF_HEADER_H

#include "ns3/header.h"
#include "ns3/ipv4-address.h"
#include "ospf-lsa.h"
#include <vector>
#include <iostream>

namespace ns3 {

/**
 * \ingroup ospf
 * \brief OSPFv2 Common Header (24 bytes) as defined in RFC 2328 Section A.3.1.
 */
class OspfHeader : public Header
{
public:
  OspfHeader();
  virtual ~OspfHeader() override;

  static TypeId GetTypeId(void);
  virtual TypeId GetInstanceTypeId(void) const override;
  virtual uint32_t GetSerializedSize(void) const override;
  virtual void Serialize(Buffer::Iterator start) const override;
  virtual uint32_t Deserialize(Buffer::Iterator start) override;
  virtual void Print(std::ostream &os) const override;

  // Setters and Getters
  void SetType(uint8_t type) { m_type = type; }
  uint8_t GetType() const { return m_type; }

  void SetPacketLength(uint16_t length) { m_packetLength = length; }
  uint16_t GetPacketLength() const { return m_packetLength; }

  void SetRouterId(Ipv4Address rtrId) { m_routerId = rtrId; }
  Ipv4Address GetRouterId() const { return m_routerId; }

  void SetAreaId(Ipv4Address areaId) { m_areaId = areaId; }
  Ipv4Address GetAreaId() const { return m_areaId; }

private:
  uint8_t m_version; ///< OSPF Version (always 2)
  uint8_t m_type; ///< Packet Type (1-5)
  uint16_t m_packetLength; ///< Total length of OSPF packet
  Ipv4Address m_routerId; ///< Router ID
  Ipv4Address m_areaId; ///< Area ID
  uint16_t m_checksum; ///< Packet Checksum
  uint16_t m_auType; ///< Authentication Type (Null = 0)
  uint8_t m_authentication[8]; ///< Authentication data
};

/**
 * \ingroup ospf
 * \brief OSPFv2 Hello Packet Payload (RFC 2328 Section A.3.2).
 */
class OspfHelloHeader : public Header
{
public:
  OspfHelloHeader();
  virtual ~OspfHelloHeader() override;

  static TypeId GetTypeId(void);
  virtual TypeId GetInstanceTypeId(void) const override;
  virtual uint32_t GetSerializedSize(void) const override;
  virtual void Serialize(Buffer::Iterator start) const override;
  virtual uint32_t Deserialize(Buffer::Iterator start) override;
  virtual void Print(std::ostream &os) const override;

  // Configuration getters and setters
  void SetNetworkMask(Ipv4Address mask) { m_networkMask = mask; }
  Ipv4Address GetNetworkMask() const { return m_networkMask; }

  void SetHelloInterval(uint16_t interval) { m_helloInterval = interval; }
  uint16_t GetHelloInterval() const { return m_helloInterval; }

  void SetOptions(uint8_t options) { m_options = options; }
  uint8_t GetOptions() const { return m_options; }

  void SetRouterPriority(uint8_t priority) { m_rtrPriority = priority; }
  uint8_t GetRouterPriority() const { return m_rtrPriority; }

  void SetRouterDeadInterval(uint32_t interval) { m_routerDeadInterval = interval; }
  uint32_t GetRouterDeadInterval() const { return m_routerDeadInterval; }

  void SetDesignatedRouter(Ipv4Address dr) { m_designatedRouter = dr; }
  Ipv4Address GetDesignatedRouter() const { return m_designatedRouter; }

  void SetBackupDesignatedRouter(Ipv4Address bdr) { m_backupDesignatedRouter = bdr; }
  Ipv4Address GetBackupDesignatedRouter() const { return m_backupDesignatedRouter; }

  void AddNeighbor(Ipv4Address neighbor) { m_neighbors.push_back(neighbor); }
  const std::vector<Ipv4Address>& GetNeighbors() const { return m_neighbors; }
  void ClearNeighbors() { m_neighbors.clear(); }

private:
  Ipv4Address m_networkMask; ///< Network Mask
  uint16_t m_helloInterval; ///< Hello Interval
  uint8_t m_options; ///< OSPF Options
  uint8_t m_rtrPriority; ///< Router Priority
  uint32_t m_routerDeadInterval; ///< Router Dead Interval
  Ipv4Address m_designatedRouter; ///< Designated Router ID
  Ipv4Address m_backupDesignatedRouter; ///< Backup Designated Router ID
  std::vector<Ipv4Address> m_neighbors; ///< Neighbor List
};

/**
 * \ingroup ospf
 * \brief OSPFv2 Database Description Packet Payload (RFC 2328 Section A.3.3).
 */
class OspfDatabaseDescriptionHeader : public Header
{
public:
  OspfDatabaseDescriptionHeader();
  virtual ~OspfDatabaseDescriptionHeader() override;

  static TypeId GetTypeId(void);
  virtual TypeId GetInstanceTypeId(void) const override;
  virtual uint32_t GetSerializedSize(void) const override;
  virtual void Serialize(Buffer::Iterator start) const override;
  virtual uint32_t Deserialize(Buffer::Iterator start) override;
  virtual void Print(std::ostream &os) const override;

  void SetInterfaceMtu(uint16_t mtu) { m_interfaceMtu = mtu; }
  uint16_t GetInterfaceMtu() const { return m_interfaceMtu; }

  void SetOptions(uint8_t options) { m_options = options; }
  uint8_t GetOptions() const { return m_options; }

  void SetFlags(uint8_t flags) { m_flags = flags; }
  uint8_t GetFlags() const { return m_flags; }

  void SetDdSequenceNumber(uint32_t seqNum) { m_ddSequenceNumber = seqNum; }
  uint32_t GetDdSequenceNumber() const { return m_ddSequenceNumber; }

  void AddLsaHeader(OspfLsaHeader lsaHeader) { m_lsaHeaders.push_back(lsaHeader); }
  const std::vector<OspfLsaHeader>& GetLsaHeaders() const { return m_lsaHeaders; }
  void ClearLsaHeaders() { m_lsaHeaders.clear(); }

private:
  uint16_t m_interfaceMtu; ///< Interface MTU
  uint8_t m_options; ///< Options
  uint8_t m_flags; ///< DD Flags (I, M, MS)
  uint32_t m_ddSequenceNumber; ///< DD Sequence Number
  std::vector<OspfLsaHeader> m_lsaHeaders; ///< LSA Headers summary list
};

/**
 * \ingroup ospf
 * \brief OSPFv2 Link State Request Packet Payload (RFC 2328 Section A.3.4).
 */
class OspfLinkStateRequestHeader : public Header
{
public:
  struct RequestRecord
  {
    uint32_t m_lsType; ///< Link State Type
    Ipv4Address m_linkStateId; ///< Link State ID
    Ipv4Address m_advertisingRouter; ///< Advertising Router ID
  };

  OspfLinkStateRequestHeader();
  virtual ~OspfLinkStateRequestHeader() override;

  static TypeId GetTypeId(void);
  virtual TypeId GetInstanceTypeId(void) const override;
  virtual uint32_t GetSerializedSize(void) const override;
  virtual void Serialize(Buffer::Iterator start) const override;
  virtual uint32_t Deserialize(Buffer::Iterator start) override;
  virtual void Print(std::ostream &os) const override;

  void AddRequest(uint32_t lsType, Ipv4Address lsId, Ipv4Address advRtr)
  {
    m_requests.push_back({lsType, lsId, advRtr});
  }
  const std::vector<RequestRecord>& GetRequests() const { return m_requests; }
  void ClearRequests() { m_requests.clear(); }

private:
  std::vector<RequestRecord> m_requests; ///< List of requested LSAs
};

/**
 * \ingroup ospf
 * \brief OSPFv2 Link State Update Packet Payload (RFC 2328 Section A.3.5).
 */
class OspfLinkStateUpdateHeader : public Header
{
public:
  OspfLinkStateUpdateHeader();
  virtual ~OspfLinkStateUpdateHeader() override;

  static TypeId GetTypeId(void);
  virtual TypeId GetInstanceTypeId(void) const override;
  virtual uint32_t GetSerializedSize(void) const override;
  virtual void Serialize(Buffer::Iterator start) const override;
  virtual uint32_t Deserialize(Buffer::Iterator start) override;
  virtual void Print(std::ostream &os) const override;

  void AddRouterLsa(const OspfRouterLsa& lsa) { m_routerLsas.push_back(lsa); }
  const std::vector<OspfRouterLsa>& GetRouterLsas() const { return m_routerLsas; }

  void AddNetworkLsa(const OspfNetworkLsa& lsa) { m_networkLsas.push_back(lsa); }
  const std::vector<OspfNetworkLsa>& GetNetworkLsas() const { return m_networkLsas; }

  void ClearLsas()
  {
    m_routerLsas.clear();
    m_networkLsas.clear();
  }

private:
  std::vector<OspfRouterLsa> m_routerLsas; ///< Router LSAs inside update
  std::vector<OspfNetworkLsa> m_networkLsas; ///< Network LSAs inside update
};

/**
 * \ingroup ospf
 * \brief OSPFv2 Link State Acknowledgment Packet Payload (RFC 2328 Section A.3.6).
 */
class OspfLinkStateAcknowledgmentHeader : public Header
{
public:
  OspfLinkStateAcknowledgmentHeader();
  virtual ~OspfLinkStateAcknowledgmentHeader() override;

  static TypeId GetTypeId(void);
  virtual TypeId GetInstanceTypeId(void) const override;
  virtual uint32_t GetSerializedSize(void) const override;
  virtual void Serialize(Buffer::Iterator start) const override;
  virtual uint32_t Deserialize(Buffer::Iterator start) override;
  virtual void Print(std::ostream &os) const override;

  void AddLsaHeader(OspfLsaHeader lsaHeader) { m_lsaHeaders.push_back(lsaHeader); }
  const std::vector<OspfLsaHeader>& GetLsaHeaders() const { return m_lsaHeaders; }
  void ClearLsaHeaders() { m_lsaHeaders.clear(); }

private:
  std::vector<OspfLsaHeader> m_lsaHeaders; ///< Acknowledged LSA Headers
};

} // namespace ns3

#endif // OSPF_HEADER_H
