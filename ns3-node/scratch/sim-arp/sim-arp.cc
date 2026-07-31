#include "ns3/applications-module.h"
#include "ns3/arp-cache.h"
#include "ns3/arp-header.h"
#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/ethernet-header.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-l3-protocol.h"
#include "ns3/network-module.h"
#include "ns3/ping-helper.h"

#include "../sim-common/env-config.h"

#include <cmath>
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SimArpEvaluation");

// arping (-c N) bypasses the OS ARP cache and sends a genuine request per
// probe; ns-3's stack caches the resolution after the first one. Flushing
// the cache before each probe (FlushArpCache) makes every probe trigger a
// real ARP exchange, so latency/jitter/loss match arping's semantics: N
// independent resolutions per run. N defaults to 5 (NS3_ARP_PROBE_COUNT).

double g_arpRequestTimeMs = -1.0;
std::vector<double> g_rttSamplesMs;

// Frame size alone can't distinguish request from reply (both pad to the
// same 64-byte CSMA minimum) - check the actual ARP opcode.
bool IsArpRequest(Ptr<const Packet> packet) {
  Ptr<Packet> copy = packet->Copy();
  EthernetHeader ethHeader;
  if (!copy->RemoveHeader(ethHeader)) {
    return false;
  }
  ArpHeader arpHeader;
  return copy->PeekHeader(arpHeader) > 0 && arpHeader.IsRequest();
}

bool IsArpReply(Ptr<const Packet> packet) {
  Ptr<Packet> copy = packet->Copy();
  EthernetHeader ethHeader;
  if (!copy->RemoveHeader(ethHeader)) {
    return false;
  }
  ArpHeader arpHeader;
  return copy->PeekHeader(arpHeader) > 0 && arpHeader.IsReply();
}

void SniffTx(Ptr<const Packet> packet) {
  if (IsArpRequest(packet)) {
    // Sub-ms precision - GetMilliSeconds() truncates to int64 and would
    // read as 0 for the sub-millisecond RTTs arping actually reports.
    g_arpRequestTimeMs = Simulator::Now().GetSeconds() * 1000.0;
  }
}

void SniffRx(Ptr<const Packet> packet) {
  if (IsArpReply(packet) && g_arpRequestTimeMs >= 0.0) {
    double nowMs = Simulator::Now().GetSeconds() * 1000.0;
    g_rttSamplesMs.push_back(nowMs - g_arpRequestTimeMs);
    g_arpRequestTimeMs = -1.0;  // consumed - ignore until the next Tx
  }
}

void FlushArpCache(Ptr<Node> node, Ptr<NetDevice> dev) {
  Ptr<Ipv4L3Protocol> ipv4L3 = node->GetObject<Ipv4L3Protocol>();
  int32_t ifIndex = ipv4L3->GetInterfaceForDevice(dev);
  Ptr<ArpCache> cache = ipv4L3->GetInterface(ifIndex)->GetArpCache();
  if (cache) {
    cache->Flush();
  }
}

int main(int argc, char *argv[]) {
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(2); // Node 0: Server, Node 1: Client

  uint32_t kProbeCount = GetEnvUint("NS3_ARP_PROBE_COUNT", 5);

  CsmaHelper csma;
  // Default: 1Gbps NIC-to-NIC macvlan run; override via NS3_DATA_RATE/
  // NS3_LINK_DELAY_US in config.toml if the real link's rating/delay is known.
  csma.SetChannelAttribute("DataRate", StringValue(GetEnvStr("NS3_DATA_RATE", "1Gbps")));
  csma.SetChannelAttribute("Delay", TimeValue(MicroSeconds(GetEnvDouble("NS3_LINK_DELAY_US", 10.0))));

  NetDeviceContainer devices;
  devices = csma.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.10.0.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  // Sniff traces on Client's device (Node 1, device 0)
  Ptr<NetDevice> clientDev = devices.Get(1);
  clientDev->TraceConnectWithoutContext("MacTx", MakeCallback(&SniffTx));
  clientDev->TraceConnectWithoutContext("MacRx", MakeCallback(&SniffRx));

  // kProbeCount independent ARP resolutions: flush the client's cached entry
  // just before each ping so it can't reuse a prior resolution, then send
  // exactly one echo (Count=1).
  PingHelper ping(interfaces.GetAddress(0));
  ping.SetAttribute("VerboseMode", EnumValue(Ping::SILENT));
  ping.SetAttribute("Count", UintegerValue(1));
  for (uint32_t i = 0; i < kProbeCount; ++i) {
    double startS = 1.0 + i * 0.1;
    Simulator::Schedule(Seconds(startS - 0.01), &FlushArpCache, nodes.Get(1),
                         clientDev);
    ApplicationContainer pingApp = ping.Install(nodes.Get(1));
    pingApp.Start(Seconds(startS));
    pingApp.Stop(Seconds(startS + 0.07));
  }

  Simulator::Stop(Seconds(1.0 + kProbeCount * 0.1 + 1.0));
  Simulator::Run();

  uint32_t resolved = g_rttSamplesMs.size();
  double lossPct = 100.0 * (double) (kProbeCount - resolved) / (double) kProbeCount;

  double latencyMs = 0.0;
  double jitterMs = 0.0;
  if (resolved > 0) {
    for (double d : g_rttSamplesMs) latencyMs += d;
    latencyMs /= resolved;
    if (resolved > 1) {
      double variance = 0.0;
      for (double d : g_rttSamplesMs) variance += (d - latencyMs) * (d - latencyMs);
      variance /= resolved;
      jitterMs = std::sqrt(variance);
    }
  }

  std::cout << "NS3_METRIC latency: " << latencyMs << " ms" << std::endl;
  std::cout << "NS3_METRIC jitter: " << jitterMs << " ms" << std::endl;
  std::cout << "NS3_METRIC loss: " << lossPct << " %" << std::endl;

  Simulator::Destroy();
  return 0;
}
