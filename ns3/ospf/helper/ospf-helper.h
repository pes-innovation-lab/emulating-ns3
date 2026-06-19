#ifndef OSPF_HELPER_H
#define OSPF_HELPER_H

#include "ns3/ipv4-routing-helper.h"
#include "ns3/ospf-routing-protocol.h"

namespace ns3 {

/**
 * \ingroup ospf
 * \brief Helper to install OSPF routing protocol on nodes.
 */
class OspfHelper : public Ipv4RoutingHelper
{
public:
  OspfHelper();
  virtual ~OspfHelper() override;

  /**
   * Virtual constructor to clone this helper.
   * @return A pointer to a new OspfHelper instance.
   */
  virtual OspfHelper* Copy(void) const override;

  /**
   * Create and install the OSPF routing protocol on a node.
   * @param node The node to install OSPF on.
   * @return A Ptr to the Ipv4RoutingProtocol.
   */
  virtual Ptr<Ipv4RoutingProtocol> Create(Ptr<Node> node) const override;
};

} // namespace ns3

#endif // OSPF_HELPER_H
