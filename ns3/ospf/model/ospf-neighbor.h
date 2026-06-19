#ifndef OSPF_NEIGHBOR_H
#define OSPF_NEIGHBOR_H

#include "ns3/ipv4-address.h"
#include "ns3/event-id.h"
#include "ns3/simple-ref-count.h"
#include "ospf-lsa.h"
#include <string>
#include <vector>
#include <map>

namespace ns3 {

/**
 * \ingroup ospf
 * \brief OSPF Neighbor States as defined in RFC 2328 Section 10.1.
 */
enum class OspfNeighborState
{
  DOWN = 0,
  INIT,
  TWO_WAY,
  EXSTART,
  EXCHANGE,
  LOADING,
  FULL
};

std::string OspfNeighborStateToString(OspfNeighborState state);

/**
 * \ingroup ospf
 * \brief Represents an OSPF Neighbor relationship (RFC 2328 Section 10).
 */
class OspfNeighbor : public SimpleRefCount<OspfNeighbor>
{
public:
  OspfNeighbor();
  ~OspfNeighbor();

  // Basic Neighbor configuration/state
  Ipv4Address m_routerId; ///< Neighbor's Router ID
  Ipv4Address m_ipAddress; ///< Neighbor's IP Address
  OspfNeighborState m_state; ///< Current state of neighbor relation
  uint8_t m_priority; ///< Neighbor's Router Priority (for DR/BDR election)
  Ipv4Address m_designatedRouter; ///< Neighbor's opinion of DR
  Ipv4Address m_backupDesignatedRouter; ///< Neighbor's opinion of BDR

  // Database Synchronization state variables
  uint32_t m_ddSeqNum; ///< Sequence number of DD packets being sent/received
  bool m_isMaster; ///< True if we are Master during DD exchange
  bool m_ddFirst; ///< True if we are sending first DD packet in exchange
  uint8_t m_ddOptions; ///< Options received from neighbor in DD

  // Timers
  EventId m_inactivityTimer; ///< Inactivity timer event
  EventId m_ddRetransmitEvent; ///< DD packet retransmission timer event

  // LSA lists for synchronization
  std::vector<OspfLsaHeader> m_dbSummaryList; ///< LSAs we need to summarize to neighbor
  std::map<std::string, OspfLsaHeader> m_lsRequestList; ///< LSAs we need to request from neighbor (keyed by type-id-advRtr)
  std::map<std::string, OspfLsaHeader> m_lsRetransmitList; ///< LSAs waiting for acknowledgment

  /**
   * Helper to format key for LSA map.
   * @param type LSA type
   * @param id Link State ID
   * @param advRtr Advertising Router ID
   * @return String key
   */
  static std::string GetLsaKey(uint8_t type, Ipv4Address id, Ipv4Address advRtr);
};

} // namespace ns3

#endif // OSPF_NEIGHBOR_H
