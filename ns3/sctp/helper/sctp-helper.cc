#include "sctp-helper.h"
#include "ns3/sctp-l4-protocol.h"
#include "ns3/node.h"

namespace ns3
{

SctpHelper::SctpHelper()
{
}

void
SctpHelper::Install(Ptr<Node> node)
{
    Ptr<SctpL4Protocol> sctp = CreateObject<SctpL4Protocol>();
    node->AggregateObject(sctp);
}

void
SctpHelper::Install(NodeContainer nodes)
{
    for (auto it = nodes.Begin(); it != nodes.End(); ++it)
    {
        Install(*it);
    }
}

} // namespace ns3
