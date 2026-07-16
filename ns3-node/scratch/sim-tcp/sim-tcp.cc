#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SimTcpEvaluation");

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
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", sinkLocalAddress);
  ApplicationContainer sinkApp = packetSinkHelper.Install (nodes.Get (0));
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (6.0));

  // Client: BulkSend on Node 1 (10.10.0.2)
  // Send to server (10.10.0.1)
  AddressValue remoteAddress (InetSocketAddress (interfaces.GetAddress (0), port));
  BulkSendHelper sourceHelper ("ns3::TcpSocketFactory", Address ());
  sourceHelper.SetAttribute ("Remote", remoteAddress);
  sourceHelper.SetAttribute ("MaxBytes", UintegerValue (0)); // Unlimited
  
  ApplicationContainer sourceApp = sourceHelper.Install (nodes.Get (1));
  sourceApp.Start (Seconds (0.5));
  sourceApp.Stop (Seconds (5.5));

  Simulator::Stop (Seconds (6.0));
  Simulator::Run ();

  // Calculate throughput
  Ptr<PacketSink> sink = DynamicCast<PacketSink> (sinkApp.Get (0));
  uint64_t totalBytesReceived = sink->GetTotalRx ();
  double duration = 5.0; // Client ran for 5.5 - 0.5 = 5 seconds
  double throughputMbps = (totalBytesReceived * 8.0) / (duration * 1e6);

  std::cout << "NS3_METRIC throughput: " << throughputMbps << " Mbps" << std::endl;

  Simulator::Destroy ();
  return 0;
}
