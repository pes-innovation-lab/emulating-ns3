#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-apps-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SimDhcpEvaluation");

double g_startTimeMs = 1000.0; // Client starts at 1.0s (1000ms)
bool g_leaseObtained = false;

void LeaseObtainedCallback(const Ipv4Address &address) {
  double nowMs = Simulator::Now().GetMilliSeconds();
  double latencyMs = nowMs - g_startTimeMs;
  std::cout << "NS3_METRIC latency: " << latencyMs << " ms" << std::endl;
  std::cout << "NS3_METRIC loss: 0.0 %" << std::endl;
  g_leaseObtained = true;
}

int main(int argc, char *argv[]) {
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(2); // Node 0: Server, Node 1: Client

  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
  csma.SetChannelAttribute("Delay", StringValue("0.1ms"));

  NetDeviceContainer devices;
  devices = csma.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  // Configure DHCP Server on Node 0
  Ipv4AddressHelper address;
  address.SetBase("10.10.0.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces =
      address.Assign(devices.Get(0)); // server IP is 10.10.0.1

  DhcpHelper dhcpHelper;
  // Default "Collect" (offer-collection wait before REQUEST) is 5s, which
  // would push REQUEST past our short sim window; shrink it so a lease can
  // complete well within the run.
  dhcpHelper.SetClientAttribute("Collect", TimeValue(MilliSeconds(200)));
  // Server parameters:
  // - NetDevice on server
  // - Server IP address
  // - Pool subnet/mask
  // - Min pool IP
  // - Max pool IP
  ApplicationContainer serverApp = dhcpHelper.InstallDhcpServer(
      devices.Get(0), interfaces.GetAddress(0), Ipv4Address("10.10.0.0"),
      Ipv4Mask("255.255.255.0"), Ipv4Address("10.10.0.10"),
      Ipv4Address("10.10.0.100"));
  Time stopTime = Seconds(5.0);
  serverApp.Start(Seconds(0.0));
  serverApp.Stop(stopTime);

  // Configure DHCP Client on Node 1
  ApplicationContainer clientApp = dhcpHelper.InstallDhcpClient(devices.Get(1));
  clientApp.Start(Seconds(g_startTimeMs / 1000.0));
  clientApp.Stop(stopTime);

  // Connect the trace
  Ptr<DhcpClient> client = DynamicCast<DhcpClient>(clientApp.Get(0));
  client->TraceConnectWithoutContext("NewLease",
                                     MakeCallback(&LeaseObtainedCallback));

  Simulator::Stop(stopTime);
  Simulator::Run();

  if (!g_leaseObtained) {
    // If we never got a lease, log 100% loss
    std::cout << "NS3_METRIC latency: 0.0 ms" << std::endl;
    std::cout << "NS3_METRIC loss: 100.0 %" << std::endl;
  }

  Simulator::Destroy();
  return 0;
}
