#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/v4ping-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SimArpEvaluation");

double g_arpRequestTimeMs = 0.0;
bool g_arpResolved = false;

void SniffTx (Ptr<const Packet> packet)
{
  // ARP packets on CSMA/Ethernet are typically 42 bytes (excluding preamble/FCS)
  // or padded to 60 bytes.
  if (packet->GetSize () == 42 || packet->GetSize () == 60)
    {
      g_arpRequestTimeMs = Simulator::Now ().GetMilliSeconds ();
    }
}

void SniffRx (Ptr<const Packet> packet)
{
  if (packet->GetSize () == 42 || packet->GetSize () == 60)
    {
      if (g_arpRequestTimeMs > 0.0 && !g_arpResolved)
        {
          double nowMs = Simulator::Now ().GetMilliSeconds ();
          double rttMs = nowMs - g_arpRequestTimeMs;
          std::cout << "NS3_METRIC latency: " << rttMs << " ms" << std::endl;
          std::cout << "NS3_METRIC loss: 0.0 %" << std::endl;
          g_arpResolved = true;
        }
    }
}

int main (int argc, char *argv[])
{
  CommandLine cmd (__FILE__);
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (2); // Node 0: Server, Node 1: Client

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("1Gbps"));
  csma.SetChannelAttribute ("Delay", StringValue ("0.1ms"));

  NetDeviceContainer devices;
  devices = csma.Install (nodes);

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.10.0.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // Connect sniffer traces to Client's device (Node 1, device 0)
  Ptr<NetDevice> clientDev = devices.Get (1);
  clientDev->TraceConnectWithoutContext ("MacTx", MakeCallback (&SniffTx));
  clientDev->TraceConnectWithoutContext ("MacRx", MakeCallback (&SniffRx));

  // Trigger an ARP request by sending a Ping from Node 1 to Node 0 at 1.0s
  V4PingHelper ping (interfaces.GetAddress (0));
  ping.SetAttribute ("Verbose", BooleanValue (false));
  ApplicationContainer pingApp = ping.Install (nodes.Get (1));
  pingApp.Start (Seconds (1.0));
  pingApp.Stop (Seconds (2.0));

  Simulator::Stop (Seconds (3.0));
  Simulator::Run ();

  if (!g_arpResolved)
    {
      std::cout << "NS3_METRIC latency: 0.0 ms" << std::endl;
      std::cout << "NS3_METRIC loss: 100.0 %" << std::endl;
    }

  Simulator::Destroy ();
  return 0;
}
