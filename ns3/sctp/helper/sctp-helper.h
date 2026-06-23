#ifndef SCTP_HELPER_H
#define SCTP_HELPER_H

#include "ns3/node-container.h"

namespace ns3
{

/**
 * @ingroup sctp
 * @brief Helper class to install SCTP transport protocol on nodes
 */
class SctpHelper
{
  public:
    SctpHelper();

    /**
     * @brief Install SCTP stack on a single Node
     * @param node The Node
     */
    void Install(Ptr<Node> node);

    /**
     * @brief Install SCTP stack on a set of Nodes
     * @param nodes The NodeContainer
     */
    void Install(NodeContainer nodes);
};

} // namespace ns3

#endif // SCTP_HELPER_H
