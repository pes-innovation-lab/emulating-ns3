#include "ospf-routing-protocol.h"
#include "ns3/log.h"
#include "ns3/ipv4-route.h"
#include "ns3/inet-socket-address.h"
#include "ns3/ipv4-packet-info-tag.h"
#include "ns3/simulator.h"
#include "ns3/uinteger.h"
#include "ns3/boolean.h"
#include "ns3/ipv4-raw-socket-factory.h"
#include <queue>
#include <set>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("OspfRoutingProtocol");

NS_OBJECT_ENSURE_REGISTERED(OspfRoutingProtocol);

TypeId
OspfRoutingProtocol::GetTypeId(void)
{
  static TypeId tid = TypeId("ns3::OspfRoutingProtocol")
    .SetParent<Ipv4RoutingProtocol>()
    .SetGroupName("Ospf")
    .AddConstructor<OspfRoutingProtocol>()
    .AddAttribute("RouterId",
                  "OSPF Router ID (IPv4 address representation).",
                  Ipv4AddressValue(Ipv4Address::GetAny()),
                  MakeIpv4AddressAccessor(&OspfRoutingProtocol::m_routerId),
                  MakeIpv4AddressChecker())
    .AddAttribute("AreaId",
                  "OSPF Area ID (defaults to backbone 0.0.0.0).",
                  Ipv4AddressValue(Ipv4Address::GetAny()),
                  MakeIpv4AddressAccessor(&OspfRoutingProtocol::m_areaId),
                  MakeIpv4AddressChecker())
    .AddAttribute("HelloInterval",
                  "Interval in seconds between sending Hello packets.",
                  UintegerValue(10),
                  MakeUintegerAccessor(&OspfRoutingProtocol::m_helloInterval),
                  MakeUintegerChecker<uint16_t>())
    .AddAttribute("RouterDeadInterval",
                  "Dead interval in seconds before declaring a neighbor down.",
                  UintegerValue(40),
                  MakeUintegerAccessor(&OspfRoutingProtocol::m_routerDeadInterval),
                  MakeUintegerChecker<uint32_t>())
    .AddAttribute("DefaultCost",
                  "Default cost metric for OSPF interfaces.",
                  UintegerValue(10),
                  MakeUintegerAccessor(&OspfRoutingProtocol::m_defaultCost),
                  MakeUintegerChecker<uint16_t>())
  ;
  return tid;
}

OspfRoutingProtocol::OspfRoutingProtocol()
  : m_routerId(Ipv4Address::GetAny()),
    m_areaId(Ipv4Address::GetAny()),
    m_helloInterval(10),
    m_routerDeadInterval(40),
    m_defaultCost(10),
    m_ipv4(nullptr),
    m_started(false)
{}

OspfRoutingProtocol::~OspfRoutingProtocol()
{}

void
OspfRoutingProtocol::DoDispose(void)
{
  m_ipv4 = nullptr;
  for (auto& pair : m_interfaces)
    {
      if (pair.second.m_socket)
        {
          pair.second.m_socket->Close();
          pair.second.m_socket = nullptr;
        }
    }
  m_interfaces.clear();
  m_neighbors.clear();
  Ipv4RoutingProtocol::DoDispose();
}

void
OspfRoutingProtocol::SetIpv4(Ptr<Ipv4> ipv4)
{
  NS_LOG_FUNCTION(this << ipv4);
  m_ipv4 = ipv4;
}

void
OspfRoutingProtocol::StartProtocol()
{
  m_started = true;
}

int
OspfRoutingProtocol::GetInterfaceForNeighbor(Ipv4Address neighborIp) const
{
  for (const auto& pair : m_interfaces)
    {
      const OspfInterface& oif = pair.second;
      if (neighborIp.CombineMask(oif.m_mask) == oif.m_ipAddress.CombineMask(oif.m_mask))
        {
          return oif.m_interfaceId;
        }
    }
  return -1;
}

void
OspfRoutingProtocol::NotifyInterfaceUp(uint32_t interface)
{
  NS_LOG_FUNCTION(this << interface);
  if (!m_ipv4)
    return;

  Ipv4InterfaceAddress ifAddr = m_ipv4->GetAddress(interface, 0);
  Ipv4Address ip = ifAddr.GetLocal();
  Ipv4Mask mask = ifAddr.GetMask();

  // Handle default Router ID selection if unset
  if (m_routerId == Ipv4Address::GetAny() || m_routerId == Ipv4Address("0.0.0.0"))
    {
      m_routerId = ip;
      NS_LOG_INFO("Auto-assigned Router ID to " << m_routerId);
    }

  StartProtocol();

  OspfInterface oif;
  oif.m_interfaceId = interface;
  oif.m_ipAddress = ip;
  oif.m_mask = mask;
  oif.m_metric = m_defaultCost;
  oif.m_helloInterval = m_helloInterval;
  oif.m_routerDeadInterval = m_routerDeadInterval;
  oif.m_routerPriority = 1;
  oif.m_designatedRouter = Ipv4Address::GetAny();
  oif.m_backupDesignatedRouter = Ipv4Address::GetAny();

  // Create socket specifically bound to this interface and NetDevice (to support multicast)
  Ptr<Socket> socket = Socket::CreateSocket(m_ipv4->GetObject<Node>(), Ipv4RawSocketFactory::GetTypeId());
  socket->SetAttribute("Protocol", UintegerValue(89));
  socket->Bind(InetSocketAddress(Ipv4Address::GetAny(), 0));
  socket->BindToNetDevice(m_ipv4->GetNetDevice(interface));
  socket->SetRecvCallback(MakeCallback(&OspfRoutingProtocol::ReceiveOspfPacket, this));
  socket->SetRecvPktInfo(true);
  oif.m_socket = socket;

  // Schedule hello sending loop
  oif.m_helloTimer = Simulator::Schedule(Seconds(0.1 + (m_routerId.Get() % 100) * 0.005),
                                         &OspfRoutingProtocol::SendHello, this, interface);

  m_interfaces[interface] = oif;

  UpdateLocalRouterLsa();
  RunDijkstra();
}

void
OspfRoutingProtocol::NotifyInterfaceDown(uint32_t interface)
{
  NS_LOG_FUNCTION(this << interface);
  auto it = m_interfaces.find(interface);
  if (it != m_interfaces.end())
    {
      it->second.m_helloTimer.Cancel();
      if (it->second.m_socket)
        {
          it->second.m_socket->Close();
        }
      m_interfaces.erase(it);
    }

  // Set any neighbors connected to this interface to DOWN
  for (auto& neighbor : m_neighbors)
    {
      if (GetInterfaceForNeighbor(neighbor->m_ipAddress) == (int)interface)
        {
          TransitionToState(neighbor, OspfNeighborState::DOWN);
        }
    }

  UpdateLocalRouterLsa();
  RunDijkstra();
}

void
OspfRoutingProtocol::NotifyAddAddress(uint32_t interface, Ipv4InterfaceAddress address)
{
  NS_LOG_FUNCTION(this << interface << address);
}

void
OspfRoutingProtocol::NotifyRemoveAddress(uint32_t interface, Ipv4InterfaceAddress address)
{
  NS_LOG_FUNCTION(this << interface << address);
}

void
OspfRoutingProtocol::ReceiveOspfPacket(Ptr<Socket> socket)
{
  Address from;
  Ptr<Packet> packet = socket->RecvFrom(from);
  if (!packet)
    return;

  InetSocketAddress inetFrom = InetSocketAddress::ConvertFrom(from);
  Ipv4Address fromIp = inetFrom.GetIpv4();

  Ipv4PacketInfoTag tag;
  uint32_t interfaceId = 0;
  if (packet->RemovePacketTag(tag))
    {
      interfaceId = tag.GetRecvIf();
    }
  else
    {
      // Fallback: look up interface corresponding to sender subnet
      interfaceId = GetInterfaceForNeighbor(fromIp);
    }

  // Remove the IP Header prepended by the raw socket implementation
  Ipv4Header ipHeader;
  packet->RemoveHeader(ipHeader);

  ProcessOspfPacket(packet, fromIp, interfaceId);
}

void
OspfRoutingProtocol::ProcessOspfPacket(Ptr<Packet> packet, Ipv4Address fromIp, uint32_t interfaceId)
{
  OspfHeader commonHeader;
  packet->RemoveHeader(commonHeader);

  if (commonHeader.GetType() == 1) // Hello
    {
      OspfHelloHeader helloHeader;
      packet->RemoveHeader(helloHeader);
      ProcessHello(commonHeader, helloHeader, fromIp, interfaceId);
    }
  else
    {
      // For types DD, LSR, LSU, LSAck, RFC 2328 requires finding an existing neighbor
      Ptr<OspfNeighbor> neighbor = nullptr;
      for (auto& nb : m_neighbors)
        {
          if (nb->m_routerId == commonHeader.GetRouterId() || nb->m_ipAddress == fromIp)
            {
              neighbor = nb;
              break;
            }
        }

      if (!neighbor)
        {
          NS_LOG_WARN("Ignoring OSPF packet type " << (int)commonHeader.GetType()
                      << " from unknown neighbor " << fromIp);
          return;
        }

      switch (commonHeader.GetType())
        {
        case 2: // Database Description
          {
            OspfDatabaseDescriptionHeader ddHeader;
            packet->RemoveHeader(ddHeader);
            ProcessDd(commonHeader, ddHeader, neighbor);
            break;
          }
        case 3: // Link State Request
          {
            OspfLinkStateRequestHeader lsrHeader;
            packet->RemoveHeader(lsrHeader);
            ProcessLsr(commonHeader, lsrHeader, neighbor);
            break;
          }
        case 4: // Link State Update
          {
            OspfLinkStateUpdateHeader lsuHeader;
            packet->RemoveHeader(lsuHeader);
            ProcessLsu(commonHeader, lsuHeader, neighbor);
            break;
          }
        case 5: // Link State Acknowledgment
          {
            OspfLinkStateAcknowledgmentHeader ackHeader;
            packet->RemoveHeader(ackHeader);
            ProcessLsaAck(commonHeader, ackHeader, neighbor);
            break;
          }
        default:
          break;
        }
    }
}

void
OspfRoutingProtocol::SendHello(uint32_t interfaceId)
{
  auto it = m_interfaces.find(interfaceId);
  if (it == m_interfaces.end())
    return;

  OspfInterface& oif = it->second;

  OspfHeader commonHeader;
  commonHeader.SetType(1);
  commonHeader.SetRouterId(m_routerId);
  commonHeader.SetAreaId(m_areaId);

  OspfHelloHeader helloHeader;
  helloHeader.SetNetworkMask(Ipv4Address(oif.m_mask.Get()));
  helloHeader.SetHelloInterval(oif.m_helloInterval);
  helloHeader.SetRouterPriority(oif.m_routerPriority);
  helloHeader.SetRouterDeadInterval(oif.m_routerDeadInterval);
  helloHeader.SetDesignatedRouter(oif.m_designatedRouter);
  helloHeader.SetBackupDesignatedRouter(oif.m_backupDesignatedRouter);

  // Add all active neighbors on this subnet to Hello list
  for (const auto& nb : m_neighbors)
    {
      if (GetInterfaceForNeighbor(nb->m_ipAddress) == (int)interfaceId &&
          nb->m_state >= OspfNeighborState::INIT)
        {
          helloHeader.AddNeighbor(nb->m_routerId);
        }
    }

  uint16_t totalLen = commonHeader.GetSerializedSize() + helloHeader.GetSerializedSize();
  commonHeader.SetPacketLength(totalLen);

  Ptr<Packet> packet = Create<Packet>();
  packet->AddHeader(helloHeader);
  packet->AddHeader(commonHeader);

  // Multicast to 224.0.0.5 via interface-bound socket
  InetSocketAddress dest("224.0.0.5", 0);
  oif.m_socket->SendTo(packet, 0, dest);

  // Schedule next Hello packet
  oif.m_helloTimer = Simulator::Schedule(Seconds(oif.m_helloInterval),
                                         &OspfRoutingProtocol::SendHello, this, interfaceId);
}

void
OspfRoutingProtocol::ProcessHello(OspfHeader commonHeader, OspfHelloHeader helloHeader, Ipv4Address fromIp, uint32_t interfaceId)
{
  Ipv4Address fromRtrId = commonHeader.GetRouterId();
  if (fromRtrId == m_routerId)
    return; // Ignore own hello loopbacks

  NS_LOG_DEBUG("Router " << m_routerId << " received Hello from " << fromRtrId << " (IP: " << fromIp << ")");

  Ptr<OspfNeighbor> neighbor = nullptr;
  for (auto& nb : m_neighbors)
    {
      if (nb->m_routerId == fromRtrId)
        {
          neighbor = nb;
          break;
        }
    }

  if (!neighbor)
    {
      neighbor = Create<OspfNeighbor>();
      neighbor->m_routerId = fromRtrId;
      neighbor->m_ipAddress = fromIp;
      neighbor->m_state = OspfNeighborState::DOWN;
      m_neighbors.push_back(neighbor);
      NS_LOG_INFO("Discovered new OSPF neighbor " << fromRtrId << " (State: DOWN)");
    }

  // Reset inactivity timer
  neighbor->m_inactivityTimer.Cancel();
  neighbor->m_inactivityTimer = Simulator::Schedule(Seconds(helloHeader.GetRouterDeadInterval()),
                                                   &OspfRoutingProtocol::NeighborInactivityTimeout, this, neighbor);

  // Update DR/BDR opinions of neighbor
  neighbor->m_priority = helloHeader.GetRouterPriority();
  neighbor->m_designatedRouter = helloHeader.GetDesignatedRouter();
  neighbor->m_backupDesignatedRouter = helloHeader.GetBackupDesignatedRouter();

  // Evaluate transitions
  if (neighbor->m_state == OspfNeighborState::DOWN)
    {
      TransitionToState(neighbor, OspfNeighborState::INIT);
    }

  // Check if our Router ID is in neighbor's list
  bool bidirectional = false;
  for (const auto& nbId : helloHeader.GetNeighbors())
    {
      if (nbId == m_routerId)
        {
          bidirectional = true;
          break;
        }
    }

  if (bidirectional)
    {
      if (neighbor->m_state == OspfNeighborState::INIT)
        {
          TransitionToState(neighbor, OspfNeighborState::TWO_WAY);
          TransitionToState(neighbor, OspfNeighborState::EXSTART);
        }
    }
  else
    {
      if (neighbor->m_state >= OspfNeighborState::TWO_WAY)
        {
          TransitionToState(neighbor, OspfNeighborState::INIT);
        }
    }
}

void
OspfRoutingProtocol::TransitionToState(Ptr<OspfNeighbor> neighbor, OspfNeighborState newState)
{
  OspfNeighborState oldState = neighbor->m_state;
  neighbor->m_state = newState;
  NS_LOG_INFO("Neighbor " << neighbor->m_routerId << " state change: "
              << OspfNeighborStateToString(oldState) << " -> " << OspfNeighborStateToString(newState));

  if (newState == OspfNeighborState::DOWN)
    {
      neighbor->m_inactivityTimer.Cancel();
      neighbor->m_ddRetransmitEvent.Cancel();
      neighbor->m_lsRequestList.clear();
      neighbor->m_lsRetransmitList.clear();
      neighbor->m_dbSummaryList.clear();
    }
  else if (newState == OspfNeighborState::EXSTART)
    {
      neighbor->m_isMaster = (m_routerId > neighbor->m_routerId);
      neighbor->m_ddFirst = true;
      // Initialize DD sequence number
      neighbor->m_ddSeqNum = Simulator::Now().GetSeconds() * 1000 + (m_routerId.Get() % 1000);
      SendDd(neighbor);
      neighbor->m_ddRetransmitEvent.Cancel();
      neighbor->m_ddRetransmitEvent = Simulator::Schedule(Seconds(5.0),
                                                          &OspfRoutingProtocol::SendDdRetransmit, this, neighbor);
    }
  else if (newState == OspfNeighborState::EXCHANGE)
    {
      neighbor->m_ddFirst = false;
      // Build summary list of all LSAs in our database
      neighbor->m_dbSummaryList.clear();
      for (const auto& pair : m_routerLsas)
        {
          neighbor->m_dbSummaryList.push_back(pair.second.m_header);
        }
      for (const auto& pair : m_networkLsas)
        {
          neighbor->m_dbSummaryList.push_back(pair.second.m_header);
        }
      neighbor->m_ddRetransmitEvent.Cancel();
      if (neighbor->m_isMaster)
        {
          neighbor->m_ddRetransmitEvent = Simulator::Schedule(Seconds(5.0),
                                                              &OspfRoutingProtocol::SendDdRetransmit, this, neighbor);
        }
    }
  else if (newState == OspfNeighborState::LOADING)
    {
      neighbor->m_ddRetransmitEvent.Cancel();
      SendLsr(neighbor);
    }
  else if (newState == OspfNeighborState::FULL)
    {
      neighbor->m_ddRetransmitEvent.Cancel();
      UpdateLocalRouterLsa();
      RunDijkstra();
    }
}

void
OspfRoutingProtocol::NeighborInactivityTimeout(Ptr<OspfNeighbor> neighbor)
{
  NS_LOG_WARN("Neighbor " << neighbor->m_routerId << " dead interval expired");
  TransitionToState(neighbor, OspfNeighborState::DOWN);
  RunDijkstra();
}

void
OspfRoutingProtocol::SendDd(Ptr<OspfNeighbor> neighbor)
{
  OspfHeader commonHeader;
  commonHeader.SetType(2);
  commonHeader.SetRouterId(m_routerId);
  commonHeader.SetAreaId(m_areaId);

  OspfDatabaseDescriptionHeader ddHeader;
  ddHeader.SetInterfaceMtu(1500);
  
  // Send up to 5 summaries per packet to avoid exceeding MTU
  uint32_t count = 0;
  while (!neighbor->m_dbSummaryList.empty() && count < 5)
    {
      ddHeader.AddLsaHeader(neighbor->m_dbSummaryList.back());
      neighbor->m_dbSummaryList.pop_back();
      count++;
    }

  uint8_t flags = 0;
  if (neighbor->m_ddFirst) flags |= 0x04; // I-bit (Init)
  if (!neighbor->m_dbSummaryList.empty() || neighbor->m_ddFirst) flags |= 0x02; // M-bit (More)
  if (neighbor->m_isMaster) flags |= 0x01; // MS-bit (Master)
  ddHeader.SetFlags(flags);
  ddHeader.SetDdSequenceNumber(neighbor->m_ddSeqNum);

  uint16_t totalLen = commonHeader.GetSerializedSize() + ddHeader.GetSerializedSize();
  commonHeader.SetPacketLength(totalLen);

  Ptr<Packet> packet = Create<Packet>();
  packet->AddHeader(ddHeader);
  packet->AddHeader(commonHeader);

  NS_LOG_INFO("Router " << m_routerId << " sending DD to " << neighbor->m_routerId 
              << " (State: " << OspfNeighborStateToString(neighbor->m_state)
              << ", Seq: " << neighbor->m_ddSeqNum 
              << ", Flags: " << (neighbor->m_ddFirst ? "I" : "") 
              << ((!neighbor->m_dbSummaryList.empty() || neighbor->m_ddFirst) ? "M" : "") 
              << (neighbor->m_isMaster ? "MS" : "") 
              << ", Summaries: " << ddHeader.GetLsaHeaders().size() << ")");

  int interfaceId = GetInterfaceForNeighbor(neighbor->m_ipAddress);
  if (interfaceId >= 0 && m_interfaces[interfaceId].m_socket)
    {
      m_interfaces[interfaceId].m_socket->SendTo(packet, 0, InetSocketAddress(neighbor->m_ipAddress, 0));
    }
}

void
OspfRoutingProtocol::SendDdRetransmit(Ptr<OspfNeighbor> neighbor)
{
  if (neighbor->m_state == OspfNeighborState::EXSTART)
    {
      NS_LOG_INFO("Router " << m_routerId << " retransmitting EXSTART DD to " << neighbor->m_routerId);
      SendDd(neighbor);
      neighbor->m_ddRetransmitEvent = Simulator::Schedule(Seconds(5.0),
                                                          &OspfRoutingProtocol::SendDdRetransmit, this, neighbor);
    }
  else if (neighbor->m_state == OspfNeighborState::EXCHANGE && neighbor->m_isMaster)
    {
      NS_LOG_INFO("Router " << m_routerId << " retransmitting EXCHANGE DD to " << neighbor->m_routerId);
      SendDd(neighbor);
      neighbor->m_ddRetransmitEvent = Simulator::Schedule(Seconds(5.0),
                                                          &OspfRoutingProtocol::SendDdRetransmit, this, neighbor);
    }
}

void
OspfRoutingProtocol::ProcessDd(OspfHeader commonHeader, OspfDatabaseDescriptionHeader ddHeader, Ptr<OspfNeighbor> neighbor)
{
  uint8_t flags = ddHeader.GetFlags();
  bool isInit = (flags & 0x04) != 0;
  bool isMore = (flags & 0x02) != 0;
  bool isMaster = (flags & 0x01) != 0;

  NS_LOG_INFO("Router " << m_routerId << " received DD from " << neighbor->m_routerId 
              << " (State: " << OspfNeighborStateToString(neighbor->m_state) 
              << ", Seq: " << ddHeader.GetDdSequenceNumber() 
              << ", Flags: " << (isInit ? "I" : "") << (isMore ? "M" : "") << (isMaster ? "MS" : "") 
              << ", Summaries: " << ddHeader.GetLsaHeaders().size() << ")");

  if (neighbor->m_state == OspfNeighborState::EXSTART)
    {
      if (isInit && isMaster && !neighbor->m_isMaster)
        {
          // We accept neighbor as Master
          neighbor->m_isMaster = false;
          neighbor->m_ddSeqNum = ddHeader.GetDdSequenceNumber();
          neighbor->m_ddFirst = false;
          TransitionToState(neighbor, OspfNeighborState::EXCHANGE);
        }
      else if (!isInit && !isMaster && neighbor->m_isMaster && ddHeader.GetDdSequenceNumber() == neighbor->m_ddSeqNum)
        {
          // Neighbor accepted us as Master
          neighbor->m_ddFirst = false;
          TransitionToState(neighbor, OspfNeighborState::EXCHANGE);
        }
      else
        {
          NS_LOG_WARN("Ignoring DD packet in EXSTART state from " << neighbor->m_routerId);
          return;
        }
    }

  if (neighbor->m_state == OspfNeighborState::EXCHANGE)
    {
      // Process received summaries
      for (const auto& lsaHeader : ddHeader.GetLsaHeaders())
        {
          // Check if we need this LSA: either we don't have it or neighbor's sequence is higher
          bool need = true;
          if (lsaHeader.m_lsType == 1)
            {
              auto it = m_routerLsas.find(lsaHeader.m_linkStateId);
              if (it != m_routerLsas.end() && it->second.m_header.m_lsSequenceNumber >= lsaHeader.m_lsSequenceNumber)
                need = false;
            }
          else if (lsaHeader.m_lsType == 2)
            {
              auto it = m_networkLsas.find(lsaHeader.m_linkStateId);
              if (it != m_networkLsas.end() && it->second.m_header.m_lsSequenceNumber >= lsaHeader.m_lsSequenceNumber)
                need = false;
            }

          if (need)
            {
              std::string key = OspfNeighbor::GetLsaKey(lsaHeader.m_lsType, lsaHeader.m_linkStateId, lsaHeader.m_advertisingRouter);
              neighbor->m_lsRequestList[key] = lsaHeader;
            }
        }

      // Handle master/slave sequence acknowledgment
      if (neighbor->m_isMaster)
        {
          // Master sends next DD
          if (ddHeader.GetDdSequenceNumber() == neighbor->m_ddSeqNum)
            {
              neighbor->m_ddSeqNum++;
              neighbor->m_ddRetransmitEvent.Cancel();
              if (neighbor->m_dbSummaryList.empty() && !isMore)
                {
                  if (neighbor->m_lsRequestList.empty())
                    TransitionToState(neighbor, OspfNeighborState::FULL);
                  else
                    TransitionToState(neighbor, OspfNeighborState::LOADING);
                }
              else
                {
                  SendDd(neighbor);
                  neighbor->m_ddRetransmitEvent = Simulator::Schedule(Seconds(5.0),
                                                                      &OspfRoutingProtocol::SendDdRetransmit, this, neighbor);
                }
            }
        }
      else
        {
          // Slave replies immediately to Master's DD
          neighbor->m_ddSeqNum = ddHeader.GetDdSequenceNumber();
          SendDd(neighbor);
          if (neighbor->m_dbSummaryList.empty() && !isMore)
            {
              if (neighbor->m_lsRequestList.empty())
                TransitionToState(neighbor, OspfNeighborState::FULL);
              else
                TransitionToState(neighbor, OspfNeighborState::LOADING);
            }
        }
    }
}

void
OspfRoutingProtocol::SendLsr(Ptr<OspfNeighbor> neighbor)
{
  if (neighbor->m_lsRequestList.empty())
    return;

  OspfHeader commonHeader;
  commonHeader.SetType(3);
  commonHeader.SetRouterId(m_routerId);
  commonHeader.SetAreaId(m_areaId);

  OspfLinkStateRequestHeader lsrHeader;
  uint32_t count = 0;
  for (const auto& pair : neighbor->m_lsRequestList)
    {
      if (count >= 10) break; // Limit size
      lsrHeader.AddRequest(pair.second.m_lsType, pair.second.m_linkStateId, pair.second.m_advertisingRouter);
      count++;
    }

  uint16_t totalLen = commonHeader.GetSerializedSize() + lsrHeader.GetSerializedSize();
  commonHeader.SetPacketLength(totalLen);

  Ptr<Packet> packet = Create<Packet>();
  packet->AddHeader(lsrHeader);
  packet->AddHeader(commonHeader);

  int interfaceId = GetInterfaceForNeighbor(neighbor->m_ipAddress);
  if (interfaceId >= 0 && m_interfaces[interfaceId].m_socket)
    {
      m_interfaces[interfaceId].m_socket->SendTo(packet, 0, InetSocketAddress(neighbor->m_ipAddress, 0));
    }
}

void
OspfRoutingProtocol::ProcessLsr(OspfHeader commonHeader, OspfLinkStateRequestHeader lsrHeader, Ptr<OspfNeighbor> neighbor)
{
  std::vector<OspfRouterLsa> sendRouterLsas;
  std::vector<OspfNetworkLsa> sendNetworkLsas;

  for (const auto& req : lsrHeader.GetRequests())
    {
      if (req.m_lsType == 1)
        {
          auto it = m_routerLsas.find(req.m_linkStateId);
          if (it != m_routerLsas.end())
            {
              sendRouterLsas.push_back(it->second);
            }
        }
      else if (req.m_lsType == 2)
        {
          auto it = m_networkLsas.find(req.m_linkStateId);
          if (it != m_networkLsas.end())
            {
              sendNetworkLsas.push_back(it->second);
            }
        }
    }

  SendLsuDirect(neighbor, sendRouterLsas, sendNetworkLsas);
}

void
OspfRoutingProtocol::SendLsuDirect(Ptr<OspfNeighbor> neighbor, const std::vector<OspfRouterLsa>& rLsas, const std::vector<OspfNetworkLsa>& nLsas)
{
  OspfHeader commonHeader;
  commonHeader.SetType(4);
  commonHeader.SetRouterId(m_routerId);
  commonHeader.SetAreaId(m_areaId);

  OspfLinkStateUpdateHeader lsuHeader;
  for (const auto& lsa : rLsas)
    lsuHeader.AddRouterLsa(lsa);
  for (const auto& lsa : nLsas)
    lsuHeader.AddNetworkLsa(lsa);

  uint16_t totalLen = commonHeader.GetSerializedSize() + lsuHeader.GetSerializedSize();
  commonHeader.SetPacketLength(totalLen);

  Ptr<Packet> packet = Create<Packet>();
  packet->AddHeader(lsuHeader);
  packet->AddHeader(commonHeader);

  int interfaceId = GetInterfaceForNeighbor(neighbor->m_ipAddress);
  if (interfaceId >= 0 && m_interfaces[interfaceId].m_socket)
    {
      m_interfaces[interfaceId].m_socket->SendTo(packet, 0, InetSocketAddress(neighbor->m_ipAddress, 0));
    }
}

void
OspfRoutingProtocol::ProcessLsu(OspfHeader commonHeader, OspfLinkStateUpdateHeader lsuHeader, Ptr<OspfNeighbor> neighbor)
{
  bool dbChanged = false;
  std::vector<OspfLsaHeader> acks;

  // Process received Router LSAs
  for (const auto& lsa : lsuHeader.GetRouterLsas())
    {
      acks.push_back(lsa.m_header);
      std::string key = OspfNeighbor::GetLsaKey(1, lsa.m_header.m_linkStateId, lsa.m_header.m_advertisingRouter);
      neighbor->m_lsRequestList.erase(key);

      auto it = m_routerLsas.find(lsa.m_header.m_linkStateId);
      if (it == m_routerLsas.end() || it->second.m_header.m_lsSequenceNumber < lsa.m_header.m_lsSequenceNumber)
        {
          m_routerLsas[lsa.m_header.m_linkStateId] = lsa;
          dbChanged = true;
          SendLsuAllNeighbors(lsa); // Flood
        }
    }

  // Process received Network LSAs
  for (const auto& lsa : lsuHeader.GetNetworkLsas())
    {
      acks.push_back(lsa.m_header);
      std::string key = OspfNeighbor::GetLsaKey(2, lsa.m_header.m_linkStateId, lsa.m_header.m_advertisingRouter);
      neighbor->m_lsRequestList.erase(key);

      auto it = m_networkLsas.find(lsa.m_header.m_linkStateId);
      if (it == m_networkLsas.end() || it->second.m_header.m_lsSequenceNumber < lsa.m_header.m_lsSequenceNumber)
        {
          m_networkLsas[lsa.m_header.m_linkStateId] = lsa;
          dbChanged = true;
          SendLsuAllNeighbors(lsa); // Flood
        }
    }

  // Send Acknowledgment back
  for (const auto& ack : acks)
    {
      SendLsaAckDirect(neighbor, ack);
    }

  if (neighbor->m_state == OspfNeighborState::LOADING)
    {
      if (neighbor->m_lsRequestList.empty())
        TransitionToState(neighbor, OspfNeighborState::FULL);
      else
        SendLsr(neighbor);
    }

  if (dbChanged)
    {
      RunDijkstra();
    }
}

void
OspfRoutingProtocol::SendLsuAllNeighbors(const OspfRouterLsa& rLsa)
{
  for (auto& neighbor : m_neighbors)
    {
      if (neighbor->m_state == OspfNeighborState::FULL)
        {
          std::vector<OspfRouterLsa> rList = {rLsa};
          SendLsuDirect(neighbor, rList, {});
        }
    }
}

void
OspfRoutingProtocol::SendLsuAllNeighbors(const OspfNetworkLsa& nLsa)
{
  for (auto& neighbor : m_neighbors)
    {
      if (neighbor->m_state == OspfNeighborState::FULL)
        {
          std::vector<OspfNetworkLsa> nList = {nLsa};
          SendLsuDirect(neighbor, {}, nList);
        }
    }
}

void
OspfRoutingProtocol::SendLsaAckDirect(Ptr<OspfNeighbor> neighbor, const OspfLsaHeader& ackHeader)
{
  OspfHeader commonHeader;
  commonHeader.SetType(5);
  commonHeader.SetRouterId(m_routerId);
  commonHeader.SetAreaId(m_areaId);

  OspfLinkStateAcknowledgmentHeader ackPack;
  ackPack.AddLsaHeader(ackHeader);

  uint16_t totalLen = commonHeader.GetSerializedSize() + ackPack.GetSerializedSize();
  commonHeader.SetPacketLength(totalLen);

  Ptr<Packet> packet = Create<Packet>();
  packet->AddHeader(ackPack);
  packet->AddHeader(commonHeader);

  int interfaceId = GetInterfaceForNeighbor(neighbor->m_ipAddress);
  if (interfaceId >= 0 && m_interfaces[interfaceId].m_socket)
    {
      m_interfaces[interfaceId].m_socket->SendTo(packet, 0, InetSocketAddress(neighbor->m_ipAddress, 0));
    }
}

void
OspfRoutingProtocol::ProcessLsaAck(OspfHeader commonHeader, OspfLinkStateAcknowledgmentHeader ackHeader, Ptr<OspfNeighbor> neighbor)
{
  // Acknowledgment clears the retransmit list
  for (const auto& ack : ackHeader.GetLsaHeaders())
    {
      std::string key = OspfNeighbor::GetLsaKey(ack.m_lsType, ack.m_linkStateId, ack.m_advertisingRouter);
      neighbor->m_lsRetransmitList.erase(key);
    }
}

void
OspfRoutingProtocol::UpdateLocalRouterLsa()
{
  OspfRouterLsa rLsa;
  rLsa.m_header.m_lsAge = 1;
  rLsa.m_header.m_lsType = 1;
  rLsa.m_header.m_linkStateId = m_routerId;
  rLsa.m_header.m_advertisingRouter = m_routerId;

  // Increment sequence number from previous
  auto it = m_routerLsas.find(m_routerId);
  if (it != m_routerLsas.end())
    {
      rLsa.m_header.m_lsSequenceNumber = it->second.m_header.m_lsSequenceNumber + 1;
    }
  else
    {
      rLsa.m_header.m_lsSequenceNumber = 0x80000001;
    }

  rLsa.m_flags = 0; // standard router
  rLsa.m_zero = 0;

  // Add a link record for each active neighbor/interface
  for (const auto& pair : m_interfaces)
    {
      const OspfInterface& oif = pair.second;

      // Find FULL neighbor on this interface
      for (const auto& nb : m_neighbors)
        {
          if (GetInterfaceForNeighbor(nb->m_ipAddress) == (int)oif.m_interfaceId &&
              nb->m_state == OspfNeighborState::FULL)
            {
              OspfLinkRecord link;
              link.m_linkId = nb->m_routerId;
              link.m_linkData = oif.m_ipAddress;
              link.m_type = 1; // Point-to-Point link
              link.m_metric = oif.m_metric;
              rLsa.m_links.push_back(link);
            }
        }

      // Always add the subnet as a Stub network link
      OspfLinkRecord stubLink;
      stubLink.m_linkId = oif.m_ipAddress.CombineMask(oif.m_mask);
      stubLink.m_linkData = Ipv4Address(oif.m_mask.Get());
      stubLink.m_type = 3; // Stub network link
      stubLink.m_metric = oif.m_metric;
      rLsa.m_links.push_back(stubLink);
    }

  m_routerLsas[m_routerId] = rLsa;
  SendLsuAllNeighbors(rLsa);
}

void
OspfRoutingProtocol::RunDijkstra()
{
  NS_LOG_INFO("Router " << m_routerId << " running Dijkstra SPF calculation");
  m_routingTable.clear();

  // Shortest path tree calculations maps
  std::map<Ipv4Address, uint32_t> dist;
  std::map<Ipv4Address, Ipv4Address> parent;
  std::map<Ipv4Address, uint32_t> outInterface;
  std::map<Ipv4Address, Ipv4Address> nextHop;

  std::set<Ipv4Address> visited;

  dist[m_routerId] = 0;
  nextHop[m_routerId] = Ipv4Address::GetAny();

  std::vector<OspfRouteEntry> rawRoutes;

  while (true)
    {
      // Find closest unvisited node u
      Ipv4Address u = Ipv4Address::GetAny();
      uint32_t minDist = 0xFFFFFFFF;
      for (const auto& pair : dist)
        {
          if (visited.find(pair.first) == visited.end() && pair.second < minDist)
            {
              minDist = pair.second;
              u = pair.first;
            }
        }

      if (u == Ipv4Address::GetAny())
        break; // All reachable nodes visited

      visited.insert(u);

      // Fetch Router LSA for node u
      auto it = m_routerLsas.find(u);
      if (it == m_routerLsas.end())
        continue;

      const OspfRouterLsa& lsa = it->second;
      for (const auto& link : lsa.m_links)
        {
          if (link.m_type == 1) // Point-to-Point connection
            {
              Ipv4Address v = link.m_linkId;
              uint32_t cost = dist[u] + link.m_metric;
              if (dist.find(v) == dist.end() || cost < dist[v])
                {
                  dist[v] = cost;
                  parent[v] = u;

                  if (u == m_routerId)
                    {
                      // Direct neighbor next hop: find the neighbor's IP address
                      Ipv4Address nhIp = link.m_linkId; // Fallback
                      for (const auto& nb : m_neighbors)
                        {
                          if (nb->m_routerId == v)
                            {
                              nhIp = nb->m_ipAddress;
                              break;
                            }
                        }
                      nextHop[v] = nhIp;
                      // Find matching local interface
                      uint32_t outIf = 0;
                      for (const auto& pairInt : m_interfaces)
                        {
                          if (pairInt.second.m_ipAddress == link.m_linkData)
                            {
                              outIf = pairInt.first;
                              break;
                            }
                        }
                      outInterface[v] = outIf;
                    }
                  else
                    {
                      nextHop[v] = nextHop[u];
                      outInterface[v] = outInterface[u];
                    }
                }
            }
          else if (link.m_type == 3) // Stub Network
            {
              // Add route to network stub directly
              OspfRouteEntry entry;
              entry.m_dest = link.m_linkId;
              entry.m_mask = Ipv4Mask(link.m_linkData.Get());
              entry.m_metric = dist[u] + link.m_metric;
              
              if (u == m_routerId)
                {
                  entry.m_nextHop = Ipv4Address::GetAny(); // Direct
                  // Find matching interface
                  uint32_t outIf = 0;
                  for (const auto& pairInt : m_interfaces)
                    {
                      if (pairInt.second.m_ipAddress.CombineMask(pairInt.second.m_mask) == link.m_linkId)
                        {
                          outIf = pairInt.first;
                          break;
                        }
                    }
                  entry.m_interfaceId = outIf;
                }
              else
                {
                  entry.m_nextHop = nextHop[u];
                  entry.m_interfaceId = outInterface[u];
                }
              rawRoutes.push_back(entry);
            }
        }
    }

  // Also add direct subnet routes for OSPF interfaces
  for (const auto& pair : m_interfaces)
    {
      const OspfInterface& oif = pair.second;
      OspfRouteEntry directEntry;
      directEntry.m_dest = oif.m_ipAddress.CombineMask(oif.m_mask);
      directEntry.m_mask = oif.m_mask;
      directEntry.m_nextHop = Ipv4Address::GetAny();
      directEntry.m_interfaceId = oif.m_interfaceId;
      directEntry.m_metric = oif.m_metric;
      rawRoutes.push_back(directEntry);
    }

  // Filter rawRoutes to only keep the best route (shortest path) for each unique destination subnet
  for (const auto& entry : rawRoutes)
    {
      bool duplicate = false;
      for (auto& existing : m_routingTable)
        {
          if (existing.m_dest == entry.m_dest && existing.m_mask == entry.m_mask)
            {
              duplicate = true;
              if (entry.m_metric < existing.m_metric)
                {
                  existing = entry; // Prefer lower metric
                }
              break;
            }
        }
      if (!duplicate)
        {
          m_routingTable.push_back(entry);
        }
    }
}

Ptr<Ipv4Route>
OspfRoutingProtocol::RouteOutput(Ptr<Packet> packet,
                                 const Ipv4Header &header,
                                 Ptr<NetDevice> oif,
                                 Socket::SocketErrno &sockerr)
{
  NS_LOG_FUNCTION(this << packet << &header << oif);

  Ipv4Address dest = header.GetDestination();

  // 1. Multicast and Broadcast packets (Hello packets, etc.)
  if (dest.IsMulticast() || dest.IsBroadcast())
    {
      Ptr<NetDevice> dev = oif;
      if (!dev)
        {
          if (m_interfaces.empty())
            {
              sockerr = Socket::ERROR_NOROUTETOHOST;
              return nullptr;
            }
          dev = m_ipv4->GetNetDevice(m_interfaces.begin()->first);
        }

      uint32_t interfaceId = m_ipv4->GetInterfaceForDevice(dev);
      Ptr<Ipv4Route> route = Create<Ipv4Route>();
      route->SetDestination(dest);
      route->SetGateway(Ipv4Address::GetAny());
      route->SetOutputDevice(dev);
      route->SetSource(m_interfaces[interfaceId].m_ipAddress);

      sockerr = Socket::ERROR_NOTERROR;
      return route;
    }

  // 2. Directly connected subnets (DD exchange, LSR, etc. before full convergence)
  for (const auto& pair : m_interfaces)
    {
      const OspfInterface& oiface = pair.second;
      if (dest.CombineMask(oiface.m_mask) == oiface.m_ipAddress.CombineMask(oiface.m_mask))
        {
          Ptr<Ipv4Route> route = Create<Ipv4Route>();
          route->SetDestination(dest);
          route->SetGateway(Ipv4Address::GetAny());
          route->SetOutputDevice(m_ipv4->GetNetDevice(oiface.m_interfaceId));
          route->SetSource(oiface.m_ipAddress);

          sockerr = Socket::ERROR_NOTERROR;
          return route;
        }
    }

  // 3. Normal routing table lookup
  OspfRouteEntry bestRoute;
  bool found = false;

  for (const auto& entry : m_routingTable)
    {
      if (dest.CombineMask(entry.m_mask) == entry.m_dest)
        {
          if (!found || 
              entry.m_mask.GetPrefixLength() > bestRoute.m_mask.GetPrefixLength() ||
              (entry.m_mask.GetPrefixLength() == bestRoute.m_mask.GetPrefixLength() && entry.m_metric < bestRoute.m_metric))
            {
              bestRoute = entry;
              found = true;
            }
        }
    }

  if (found)
    {
      Ptr<Ipv4Route> route = Create<Ipv4Route>();
      route->SetDestination(bestRoute.m_dest);
      route->SetGateway(bestRoute.m_nextHop == Ipv4Address::GetAny() ? dest : bestRoute.m_nextHop);
      route->SetOutputDevice(m_ipv4->GetNetDevice(bestRoute.m_interfaceId));
      route->SetSource(m_interfaces[bestRoute.m_interfaceId].m_ipAddress);

      sockerr = Socket::ERROR_NOTERROR;
      return route;
    }

  sockerr = Socket::ERROR_NOROUTETOHOST;
  return nullptr;
}

bool
OspfRoutingProtocol::RouteInput(Ptr<const Packet> packet,
                                const Ipv4Header &header,
                                Ptr<const NetDevice> iif,
                                const UnicastForwardCallback &ucb,
                                const MulticastForwardCallback &mcb,
                                const LocalDeliverCallback &lcb,
                                const ErrorCallback &ecb)
{
  NS_LOG_FUNCTION(this << packet << &header << iif);

  Ipv4Address dest = header.GetDestination();
  uint32_t interfaceId = m_ipv4->GetInterfaceForDevice(iif);

  // Check if packet is destined for this node
  for (uint32_t i = 0; i < m_ipv4->GetNInterfaces(); ++i)
    {
      for (uint32_t j = 0; j < m_ipv4->GetNAddresses(i); ++j)
        {
          Ipv4InterfaceAddress ifAddr = m_ipv4->GetAddress(i, j);
          if (dest == ifAddr.GetLocal())
            {
              lcb(packet, header, interfaceId);
              return true;
            }
        }
    }

  // Check if packet is destined for AllSPFRouters multicast (224.0.0.5)
  if (dest == Ipv4Address("224.0.0.5"))
    {
      lcb(packet, header, interfaceId);
      return true;
    }

  // Forward packet
  OspfRouteEntry bestRoute;
  bool found = false;

  for (const auto& entry : m_routingTable)
    {
      if (dest.CombineMask(entry.m_mask) == entry.m_dest)
        {
          if (!found || 
              entry.m_mask.GetPrefixLength() > bestRoute.m_mask.GetPrefixLength() ||
              (entry.m_mask.GetPrefixLength() == bestRoute.m_mask.GetPrefixLength() && entry.m_metric < bestRoute.m_metric))
            {
              bestRoute = entry;
              found = true;
            }
        }
    }

  if (found)
    {
      Ptr<Ipv4Route> route = Create<Ipv4Route>();
      route->SetDestination(bestRoute.m_dest);
      route->SetGateway(bestRoute.m_nextHop == Ipv4Address::GetAny() ? dest : bestRoute.m_nextHop);
      route->SetOutputDevice(m_ipv4->GetNetDevice(bestRoute.m_interfaceId));
      route->SetSource(m_interfaces[bestRoute.m_interfaceId].m_ipAddress);

      ucb(route, packet, header);
      return true;
    }

  return false;
}

void
OspfRoutingProtocol::PrintRoutingTable(Ptr<OutputStreamWrapper> stream, Time::Unit unit) const
{
  std::ostream* os = stream->GetStream();
  *os << "OSPF Routing Table for Router ID " << m_routerId << " at time " << Simulator::Now().As(unit) << "\n";
  *os << "Destination\tGateway\t\tInterface\tMetric\n";
  for (const auto& entry : m_routingTable)
    {
      *os << entry.m_dest << "\t"
          << (entry.m_nextHop == Ipv4Address::GetAny() ? "0.0.0.0" : entry.m_nextHop) << "\t\t"
          << "Interface " << entry.m_interfaceId << "\t"
          << entry.m_metric << "\n";
    }
  *os << "\n";
}

} // namespace ns3
