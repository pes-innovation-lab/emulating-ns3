#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

#include "../sim-common/env-config.h"

#include <cmath>
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SimUdpEvaluation");

static uint64_t g_packetsSent = 0;
static uint64_t g_packetsReceived = 0;
static double g_lastRxTimeMs = -1.0;
static std::vector<double> g_interArrivalMs;

void
TxCallback (Ptr<const Packet> packet)
{
  g_packetsSent++;
}

void
RxCallback (Ptr<const Packet> packet, const Address &from)
{
  g_packetsReceived++;
  // Sub-ms precision - GetMilliSeconds() truncates
  double nowMs = Simulator::Now ().GetSeconds () * 1000.0;
  if (g_lastRxTimeMs >= 0.0)
    {
      g_interArrivalMs.push_back (nowMs - g_lastRxTimeMs);
    }
  g_lastRxTimeMs = nowMs;
}

int main (int argc, char *argv[])
{
  CommandLine cmd (__FILE__);
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);

  double durationS = GetEnvDouble ("NS3_UDP_DURATION_S", 5.0);

  NodeContainer nodes;
  nodes.Create (2);

  PointToPointHelper pointToPoint;
  // Physical link: 1Gbps NIC-to-NIC, direct macvlan on a short Ethernet run
  // by default - override via NS3_DATA_RATE/NS3_LINK_DELAY_US in config.toml
  // if the real link's actual rating/propagation delay is known.
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue (GetEnvStr ("NS3_DATA_RATE", "1Gbps")));
  pointToPoint.SetChannelAttribute (
      "Delay", TimeValue (MicroSeconds (GetEnvDouble ("NS3_LINK_DELAY_US", 10.0))));

  NetDeviceContainer devices;
  devices = pointToPoint.Install (nodes);

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.10.0.0", "255.255.255.0");

  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // Server: Packet Sink on Node 0 (10.10.0.1)
  uint16_t port = 9;
  Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
  PacketSinkHelper packetSinkHelper ("ns3::UdpSocketFactory", sinkLocalAddress);
  ApplicationContainer sinkApp = packetSinkHelper.Install (nodes.Get (0));
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (durationS + 1.0));

  // Client: OnOffApplication on Node 1 (10.10.0.2) sending to Node 0 (10.10.0.1).
  // Bitrate/packet size default to matching this bench's own iperf3 -b/-u
  // invocation (config.toml's client_cmd) - override via NS3_UDP_BITRATE_MBPS/
  // NS3_UDP_PACKET_SIZE if the real command line differs.
  std::string udpDataRate = GetEnvStr ("NS3_UDP_BITRATE_MBPS", "10") + "Mbps";
  uint32_t udpPacketSize = (uint32_t) GetEnvDouble ("NS3_UDP_PACKET_SIZE", 1472);
  OnOffHelper onOffHelper ("ns3::UdpSocketFactory", InetSocketAddress (interfaces.GetAddress (0), port));
  onOffHelper.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  onOffHelper.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  onOffHelper.SetAttribute ("DataRate", StringValue (udpDataRate));
  onOffHelper.SetAttribute ("PacketSize", UintegerValue (udpPacketSize));

  ApplicationContainer clientApps = onOffHelper.Install (nodes.Get (1));
  clientApps.Start (Seconds (0.5));
  clientApps.Stop (Seconds (0.5 + durationS));

  Ptr<PacketSink> sink = DynamicCast<PacketSink> (sinkApp.Get (0));
  sink->TraceConnectWithoutContext ("Rx", MakeCallback (&RxCallback));
  clientApps.Get (0)->TraceConnectWithoutContext ("Tx", MakeCallback (&TxCallback));

  Simulator::Stop (Seconds (durationS + 1.0));
  Simulator::Run ();

  // Calculate throughput
  uint64_t totalBytesReceived = sink->GetTotalRx ();
  double throughputMbps = (totalBytesReceived * 8.0) / (durationS * 1e6);

  double lossPct = g_packetsSent > 0
    ? (100.0 * (double) (g_packetsSent - g_packetsReceived) / (double) g_packetsSent)
    : 0.0;

  double jitterMs = 0.0;
  if (g_interArrivalMs.size () > 1)
    {
      double mean = 0.0;
      for (double d : g_interArrivalMs) mean += d;
      mean /= g_interArrivalMs.size ();
      double variance = 0.0;
      for (double d : g_interArrivalMs) variance += (d - mean) * (d - mean);
      variance /= g_interArrivalMs.size ();
      jitterMs = std::sqrt (variance);
    }

  std::cout << "NS3_METRIC throughput: " << throughputMbps << " Mbps" << std::endl;
  std::cout << "NS3_METRIC jitter: " << jitterMs << " ms" << std::endl;
  std::cout << "NS3_METRIC loss: " << lossPct << " %" << std::endl;

  Simulator::Destroy ();
  return 0;
}
