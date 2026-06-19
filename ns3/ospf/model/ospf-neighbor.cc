#include "ospf-neighbor.h"
#include <sstream>

namespace ns3 {

std::string
OspfNeighborStateToString(OspfNeighborState state)
{
  switch (state)
    {
    case OspfNeighborState::DOWN:
      return "DOWN";
    case OspfNeighborState::INIT:
      return "INIT";
    case OspfNeighborState::TWO_WAY:
      return "2-WAY";
    case OspfNeighborState::EXSTART:
      return "EXSTART";
    case OspfNeighborState::EXCHANGE:
      return "EXCHANGE";
    case OspfNeighborState::LOADING:
      return "LOADING";
    case OspfNeighborState::FULL:
      return "FULL";
    default:
      return "UNKNOWN";
    }
}

OspfNeighbor::OspfNeighbor()
  : m_routerId(Ipv4Address::GetAny()),
    m_ipAddress(Ipv4Address::GetAny()),
    m_state(OspfNeighborState::DOWN),
    m_priority(1),
    m_designatedRouter(Ipv4Address::GetAny()),
    m_backupDesignatedRouter(Ipv4Address::GetAny()),
    m_ddSeqNum(0),
    m_isMaster(true),
    m_ddFirst(true),
    m_ddOptions(0)
{}

OspfNeighbor::~OspfNeighbor()
{
  m_inactivityTimer.Cancel();
  m_ddRetransmitEvent.Cancel();
}

std::string
OspfNeighbor::GetLsaKey(uint8_t type, Ipv4Address id, Ipv4Address advRtr)
{
  std::stringstream ss;
  ss << (int)type << "-" << id << "-" << advRtr;
  return ss.str();
}

} // namespace ns3
