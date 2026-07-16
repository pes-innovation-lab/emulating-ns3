#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/ping-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SimArpEvaluation");

double g_arpRequestTimeMs = 0.0;
bool g_arpResolved = false;

// ARP payload is 28 bytes; CsmaNetDevice pads/frames it to a 64-byte
// minimum Ethernet frame
const uint32_t kArpFrameSize = 64;

void SniffTx(Ptr<const Packet> packet) {
  if (packet->GetSize() == kArpFrameSize) {
    // Sub-ms precision - GetMilliSeconds() truncates to int64 and would
    // read as 0 for the sub-millisecond RTTs arping actually reports.
    g_arpRequestTimeMs = Simulator::Now().GetSeconds() * 1000.0;
  }
}

void SniffRx(Ptr<const Packet> packet) {
  if (packet->GetSize() == kArpFrameSize) {
    if (g_arpRequestTimeMs > 0.0 && !g_arpResolved) {
      double nowMs = Simulator::Now().GetSeconds() * 1000.0;
      double rttMs = nowMs - g_arpRequestTimeMs;
      std::cout << "NS3_METRIC latency: " << rttMs << " ms" << std::endl;
      std::cout << "NS3_METRIC loss: 0.0 %" << std::endl;
      g_arpResolved = true;
    }
  }
}

int main(int argc, char *argv[]) {
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(2); // Node 0: Server, Node 1: Client

  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", StringValue("1Gbps"));
  // Delay sampled once per run (seeded via RngRun) instead of a fixed
  // 0.1ms - a fixed channel delay made every run's latency bit-identical,
  // so std_sim was always 0 regardless of num_runs.
  Ptr<UniformRandomVariable> delayRv = CreateObject<UniformRandomVariable>();
  delayRv->SetAttribute("Min", DoubleValue(0.05));
  delayRv->SetAttribute("Max", DoubleValue(0.15));
  csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(delayRv->GetValue())));

  NetDeviceContainer devices;
  devices = csma.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.10.0.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  // Connect sniffer traces to Client's device (Node 1, device 0)
  Ptr<NetDevice> clientDev = devices.Get(1);
  clientDev->TraceConnectWithoutContext("MacTx", MakeCallback(&SniffTx));
  clientDev->TraceConnectWithoutContext("MacRx", MakeCallback(&SniffRx));

  // Trigger an ARP request by sending a Ping from Node 1 to Node 0 at 1.0s
  PingHelper ping(interfaces.GetAddress(0));
  ping.SetAttribute("VerboseMode", EnumValue(Ping::SILENT));
  ApplicationContainer pingApp = ping.Install(nodes.Get(1));
  pingApp.Start(Seconds(1.0));
  pingApp.Stop(Seconds(2.0));

  Simulator::Stop(Seconds(3.0));
  Simulator::Run();

  if (!g_arpResolved) {
    std::cout << "NS3_METRIC latency: 0.0 ms" << std::endl;
    std::cout << "NS3_METRIC loss: 100.0 %" << std::endl;
  }

  Simulator::Destroy();
  return 0;
}
