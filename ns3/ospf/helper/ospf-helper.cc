#include "ospf-helper.h"

namespace ns3 {

OspfHelper::OspfHelper()
{}

OspfHelper::~OspfHelper()
{}

OspfHelper*
OspfHelper::Copy(void) const
{
  return new OspfHelper(*this);
}

Ptr<Ipv4RoutingProtocol>
OspfHelper::Create(Ptr<Node> node) const
{
  Ptr<OspfRoutingProtocol> protocol = CreateObject<OspfRoutingProtocol>();
  return protocol;
}

} // namespace ns3
