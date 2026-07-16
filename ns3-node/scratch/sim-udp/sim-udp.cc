#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SimUdpEvaluation");

int main (int argc, char *argv[])
{
  CommandLine cmd (__FILE__);
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);

  NodeContainer nodes;
  nodes.Create (2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("1Gbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("0.1ms"));

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
  sinkApp.Stop (Seconds (6.0));

  // Client: OnOffApplication on Node 1 (10.10.0.2) sending to Node 0 (10.10.0.1)
  OnOffHelper onOffHelper ("ns3::UdpSocketFactory", InetSocketAddress (interfaces.GetAddress (0), port));
  onOffHelper.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  onOffHelper.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  onOffHelper.SetAttribute ("DataRate", StringValue ("10Mbps"));
  onOffHelper.SetAttribute ("PacketSize", UintegerValue (1472)); // 1500 - 20 (IP) - 8 (UDP)

  ApplicationContainer clientApps = onOffHelper.Install (nodes.Get (1));
  clientApps.Start (Seconds (0.5));
  clientApps.Stop (Seconds (5.5));

  Simulator::Stop (Seconds (6.0));
  Simulator::Run ();

  // Calculate throughput
  Ptr<PacketSink> sink = DynamicCast<PacketSink> (sinkApp.Get (0));
  uint64_t totalBytesReceived = sink->GetTotalRx ();
  double duration = 5.0; // Client ran for 5 seconds
  double throughputMbps = (totalBytesReceived * 8.0) / (duration * 1e6);

  std::cout << "NS3_METRIC throughput: " << throughputMbps << " Mbps" << std::endl;

  Simulator::Destroy ();
  return 0;
}
