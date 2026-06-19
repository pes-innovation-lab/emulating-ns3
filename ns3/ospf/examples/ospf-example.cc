#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-apps-module.h"
#include "ns3/ospf-helper.h"
#include <iostream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("OspfExample");

int
main(int argc, char* argv[])
{
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  LogComponentEnable("OspfExample", LOG_LEVEL_INFO);
  LogComponentEnable("OspfRoutingProtocol", LOG_LEVEL_INFO);

  NS_LOG_INFO("Creating 3 nodes in a chain topology: Node A <-> Node B <-> Node C");
  NodeContainer nodes;
  nodes.Create(3);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer devicesAB = p2p.Install(nodes.Get(0), nodes.Get(1));
  NetDeviceContainer devicesBC = p2p.Install(nodes.Get(1), nodes.Get(2));

  InternetStackHelper stack;
  OspfHelper ospf;
  stack.SetRoutingHelper(ospf);
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfacesAB = address.Assign(devicesAB);

  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfacesBC = address.Assign(devicesBC);

  // Set up Ping application from Node A to Node C
  Ipv4Address destAddr = interfacesBC.GetAddress(1); // Node C IP (10.1.2.2)
  NS_LOG_INFO("Configuring Ping from Node A to Node C (" << destAddr << ")");
  
  PingHelper ping(destAddr);
  ping.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  ping.SetAttribute("Size", UintegerValue(56));

  ApplicationContainer apps = ping.Install(nodes.Get(0));
  apps.Start(Seconds(11.0)); // Start ping after adjacencies have reached FULL
  apps.Stop(Seconds(18.0));

  // Enable ascii and pcap tracing
  AsciiTraceHelper ascii;
  p2p.EnableAsciiAll(ascii.CreateFileStream("ospf-example.tr"));
  p2p.EnablePcapAll("ospf-example");

  // Print routing tables at intervals
  Ptr<OutputStreamWrapper> routingTableStream = ascii.CreateFileStream("ospf-example.routes");
  ospf.PrintRoutingTableAt(Seconds(2.0), nodes.Get(0), routingTableStream);
  ospf.PrintRoutingTableAt(Seconds(7.0), nodes.Get(0), routingTableStream);
  ospf.PrintRoutingTableAt(Seconds(12.0), nodes.Get(0), routingTableStream);

  NS_LOG_INFO("Starting Simulation.");
  Simulator::Stop(Seconds(20.0));
  Simulator::Run();
  Simulator::Destroy();
  NS_LOG_INFO("Simulation finished successfully.");
  
  return 0;
}
