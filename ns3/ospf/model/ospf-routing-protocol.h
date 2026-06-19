#ifndef OSPF_ROUTING_PROTOCOL_H
#define OSPF_ROUTING_PROTOCOL_H

#include "ns3/ipv4-routing-protocol.h"
#include "ns3/object.h"
#include "ns3/ipv4.h"
#include "ns3/node.h"
#include "ns3/socket.h"
#include "ns3/event-id.h"
#include "ospf-header.h"
#include "ospf-lsa.h"
#include "ospf-neighbor.h"
#include <map>
#include <vector>

namespace ns3 {

/**
 * \ingroup ospf
 * \brief Native single-area OSPFv2 routing protocol for IPv4 (RFC 2328).
 */
class OspfRoutingProtocol : public Ipv4RoutingProtocol
{
public:
  static TypeId GetTypeId(void);

  OspfRoutingProtocol();
  virtual ~OspfRoutingProtocol() override;

  // Inherited from Ipv4RoutingProtocol
  virtual Ptr<Ipv4Route> RouteOutput(Ptr<Packet> packet,
                                     const Ipv4Header &header,
                                     Ptr<NetDevice> oif,
                                     Socket::SocketErrno &sockerr) override;

  virtual bool RouteInput(Ptr<const Packet> packet,
                          const Ipv4Header &header,
                          Ptr<const NetDevice> iif,
                          const UnicastForwardCallback &ucb,
                          const MulticastForwardCallback &mcb,
                          const LocalDeliverCallback &lcb,
                          const ErrorCallback &ecb) override;

  virtual void NotifyInterfaceUp(uint32_t interface) override;
  virtual void NotifyInterfaceDown(uint32_t interface) override;
  virtual void NotifyAddAddress(uint32_t interface, Ipv4InterfaceAddress address) override;
  virtual void NotifyRemoveAddress(uint32_t interface, Ipv4InterfaceAddress address) override;
  virtual void SetIpv4(Ptr<Ipv4> ipv4) override;
  virtual void PrintRoutingTable(Ptr<OutputStreamWrapper> stream, Time::Unit unit = Time::S) const override;

protected:
  virtual void DoDispose(void) override;

private:
  // OSPF internal data structures
  struct OspfInterface
  {
    uint32_t m_interfaceId; ///< Interface ID in ns-3
    Ipv4Address m_ipAddress; ///< IP address of interface
    Ipv4Mask m_mask; ///< Subnet Mask
    uint16_t m_metric; ///< OSPF cost metric
    uint16_t m_helloInterval; ///< Hello interval in seconds
    uint32_t m_routerDeadInterval; ///< Dead interval in seconds
    uint8_t m_routerPriority; ///< Router Priority for DR election
    Ipv4Address m_designatedRouter; ///< Designated Router
    Ipv4Address m_backupDesignatedRouter; ///< Backup Designated Router
    EventId m_helloTimer; ///< Scheduled Hello sender event
    Ptr<Socket> m_socket; ///< Raw OSPF socket bound to this interface
  };

  void StartProtocol();
  int GetInterfaceForNeighbor(Ipv4Address neighborIp) const;
  void ReceiveOspfPacket(Ptr<Socket> socket);
  void ProcessOspfPacket(Ptr<Packet> packet, Ipv4Address fromIp, uint32_t interfaceId);

  // Packet senders
  void SendHello(uint32_t interfaceId);
  void SendDd(Ptr<OspfNeighbor> neighbor);
  void SendDdRetransmit(Ptr<OspfNeighbor> neighbor);
  void SendLsr(Ptr<OspfNeighbor> neighbor);
  void SendLsuAllNeighbors(const OspfRouterLsa& rLsa);
  void SendLsuAllNeighbors(const OspfNetworkLsa& nLsa);
  void SendLsuDirect(Ptr<OspfNeighbor> neighbor, const std::vector<OspfRouterLsa>& rLsas, const std::vector<OspfNetworkLsa>& nLsas);
  void SendLsaAckDirect(Ptr<OspfNeighbor> neighbor, const OspfLsaHeader& ackHeader);

  // Packet processors
  void ProcessHello(OspfHeader commonHeader, OspfHelloHeader helloHeader, Ipv4Address fromIp, uint32_t interfaceId);
  void ProcessDd(OspfHeader commonHeader, OspfDatabaseDescriptionHeader ddHeader, Ptr<OspfNeighbor> neighbor);
  void ProcessLsr(OspfHeader commonHeader, OspfLinkStateRequestHeader lsrHeader, Ptr<OspfNeighbor> neighbor);
  void ProcessLsu(OspfHeader commonHeader, OspfLinkStateUpdateHeader lsuHeader, Ptr<OspfNeighbor> neighbor);
  void ProcessLsaAck(OspfHeader commonHeader, OspfLinkStateAcknowledgmentHeader ackHeader, Ptr<OspfNeighbor> neighbor);

  // DR/BDR election
  void RunDrElection(uint32_t interfaceId);

  // Timers and State transitions
  void NeighborInactivityTimeout(Ptr<OspfNeighbor> neighbor);
  void TransitionToState(Ptr<OspfNeighbor> neighbor, OspfNeighborState newState);

  // Database and routing (SPF)
  void UpdateLocalRouterLsa();
  void UpdateLocalNetworkLsa(uint32_t interfaceId);
  void RunDijkstra();

  // Attributes / Configuration variables
  Ipv4Address m_routerId; ///< Configured Router ID
  Ipv4Address m_areaId; ///< OSPF Area ID (0.0.0.0 for single area)
  uint16_t m_helloInterval; ///< Default hello interval
  uint32_t m_routerDeadInterval; ///< Default dead interval
  uint16_t m_defaultCost; ///< Default metric cost

  Ptr<Ipv4> m_ipv4; ///< Pointer to associated IPv4 stack
  bool m_started; ///< True if OSPF protocol has started

  std::map<uint32_t, OspfInterface> m_interfaces; ///< List of active OSPF interfaces
  std::vector<Ptr<OspfNeighbor>> m_neighbors; ///< Active neighbors list

  // Link State Database (LSDB)
  std::map<Ipv4Address, OspfRouterLsa> m_routerLsas; ///< Type 1 LSAs keyed by Advertising Router ID
  std::map<Ipv4Address, OspfNetworkLsa> m_networkLsas; ///< Type 2 LSAs keyed by Link State ID (DR's interface IP)

  // Internal OSPF Routing Table
  struct OspfRouteEntry
  {
    Ipv4Address m_dest; ///< Destination network
    Ipv4Mask m_mask; ///< Netmask
    Ipv4Address m_nextHop; ///< Next-hop IP
    uint32_t m_interfaceId; ///< Output interface ID
    uint32_t m_metric; ///< Metric cost to destination
  };
  std::vector<OspfRouteEntry> m_routingTable; ///< Computed routing table entries
};

} // namespace ns3

#endif // OSPF_ROUTING_PROTOCOL_H
