#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/fd-net-device-module.h"
#include "ns3/inet-socket-address.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/log.h"
#include "ns3/network-module.h"

#include <fstream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DockerUDPEcho");

int main(int argc, char *argv[]) {
  GlobalValue::Bind("SimulatorImplementationType",
                    StringValue("ns3::RealtimeSimulatorImpl"));
  GlobalValue::Bind("ChecksumEnabled", BooleanValue(true));

  std::string clientIp = "10.99.1.22";
  std::string ns3Ip = "10.99.1.11";
  int port = 1234;

  LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_ALL);
  LogComponentEnable("ArpL3Protocol", LOG_LEVEL_ALL);
  LogComponentEnable("Ipv4L3Protocol", LOG_LEVEL_ALL);

  NodeContainer nodes;
  nodes.Create(1);

  EmuFdNetDeviceHelper emu;
  emu.SetDeviceName("eth0");
  NetDeviceContainer devices = emu.Install(nodes.Get(0));

  // workaround to allow for inbound traffic, ns-3 needs to set the node's mac
  // address as it drops the packet otherwise
  std::ifstream ifs("/sys/class/net/eth0/address");
  std::string mac;
  std::getline(ifs, mac);
  devices.Get(0)->SetAddress(Mac48Address(mac.c_str()));

  emu.EnablePcapAll("ns3-docker");

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.99.1.0", "255.255.255.0", "0.0.0.11");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  Ptr<Ipv4> ipv4 = nodes.Get(0)->GetObject<Ipv4>();
  NS_LOG_INFO("Node 0 has " << ipv4->GetNInterfaces() << " interfaces");
  for (uint32_t i = 0; i < ipv4->GetNInterfaces(); i++) {
    for (uint32_t j = 0; j < ipv4->GetNAddresses(i); j++) {
      NS_LOG_INFO("  Interface "
                  << i << " addr: " << ipv4->GetAddress(i, j).GetLocal());
    }
  }

  UdpEchoServerHelper udpHelper(port);
  Ptr<Node> node = nodes.Get(0);
  ApplicationContainer container = udpHelper.Install(node);

  // container.Start(Seconds(0.0));
  container.Stop(Seconds(60.0));

  NS_LOG_INFO("Starting Docker UDP echo simulation...");

  Simulator::Stop(Seconds(60.0));
  Simulator::Run();
  Simulator::Destroy();

  NS_LOG_INFO("Simulation Finished.");

  return 0;
}
