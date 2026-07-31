#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/ping-helper.h"
#include "ns3/point-to-point-module.h"

#include "../sim-common/env-config.h"

#include <cmath>
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SimPingBaseline");

static std::vector<double> g_rttSamplesMs;

void RttTracer(unsigned short seq, Time rtt)
{
  g_rttSamplesMs.push_back(rtt.GetSeconds() * 1000.0);
}

int main(int argc, char *argv[])
{
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);

  uint32_t pingCount = (uint32_t)GetEnvDouble("NS3_PING_COUNT", 10);

  NodeContainer nodes;
  nodes.Create(2);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue(GetEnvStr("NS3_DATA_RATE", "1Gbps")));
  p2p.SetChannelAttribute("Delay", TimeValue(MicroSeconds(GetEnvDouble("NS3_LINK_DELAY_US", 10.0))));

  NetDeviceContainer devices = p2p.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.10.0.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  PingHelper pingHelper(interfaces.GetAddress(0));
  pingHelper.SetAttribute("VerboseMode", EnumValue(Ping::SILENT));
  pingHelper.SetAttribute("Count", UintegerValue(1));

  double intervalS = 1.0;
  ApplicationContainer pingApps;
  for (uint32_t i = 0; i < pingCount; ++i)
  {
    double startS = 1.0 + i * intervalS;
    ApplicationContainer app = pingHelper.Install(nodes.Get(1));
    pingApps.Add(app);
    app.Start(Seconds(startS));
    app.Stop(Seconds(startS + 0.1));
  }

  for (uint32_t i = 0; i < pingApps.GetN(); ++i)
  {
    Ptr<Ping> p = DynamicCast<Ping>(pingApps.Get(i));
    if (p)
    {
      p->TraceConnectWithoutContext("Rtt", MakeCallback(&RttTracer));
    }
  }

  Simulator::Stop(Seconds(1.0 + pingCount * intervalS + 1.0));
  Simulator::Run();

  uint32_t received = g_rttSamplesMs.size();
  double lossPct = 100.0 * (double)(pingCount - received) / (double)pingCount;

  double latencyMs = 0.0;
  double jitterMs = 0.0;
  if (received > 0)
  {
    for (double d : g_rttSamplesMs)
      latencyMs += d;
    latencyMs /= received;
    if (received > 1)
    {
      double variance = 0.0;
      for (double d : g_rttSamplesMs)
        variance += (d - latencyMs) * (d - latencyMs);
      variance /= received;
      jitterMs = std::sqrt(variance);
    }
  }

  std::cout << "NS3_METRIC latency: " << latencyMs << " ms" << std::endl;
  std::cout << "NS3_METRIC jitter: " << jitterMs << " ms" << std::endl;
  std::cout << "NS3_METRIC loss: " << lossPct << " %" << std::endl;

  Simulator::Destroy();
  return 0;
}
