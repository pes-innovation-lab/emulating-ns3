#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/tcp-socket-base.h"

#include <cmath>
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SimTcpEvaluation");

static std::vector<double> g_rttSamplesMs;
static std::vector<double> g_cwndSamplesKB;
static uint32_t g_retransmitEvents = 0;

void
RttTracer (Time oldRtt, Time newRtt)
{
  g_rttSamplesMs.push_back (newRtt.GetSeconds () * 1000.0);
}

void
CwndTracer (uint32_t oldCwnd, uint32_t newCwnd)
{
  g_cwndSamplesKB.push_back (newCwnd / 1024.0);
}

void
CongStateTracer (TcpSocketState::TcpCongState_t oldState, TcpSocketState::TcpCongState_t newState)
{
  // ns-3 has no direct per-segment retransmit counter to match iperf3's
  // TCP_INFO retransmits. Entering CA_LOSS/CA_RECOVERY is the socket
  // detecting loss and retransmitting to recover from it, so counting
  // those transitions is a retransmission-episode proxy, not a literal
  // segment-level retransmit count.
  if (newState == TcpSocketState::CA_LOSS || newState == TcpSocketState::CA_RECOVERY)
    {
      g_retransmitEvents++;
    }
}

void
ConnectTcpTraces (uint32_t nodeId)
{
  // BulkSend's socket doesn't exist until StartApplication runs, so this
  // is scheduled to fire just after Start() instead of connecting before
  // Run() - the wildcard path only matches sockets that already exist.
  std::string base = "/NodeList/" + std::to_string (nodeId) + "/$ns3::TcpL4Protocol/SocketList/*/";
  Config::ConnectWithoutContext (base + "RTT", MakeCallback (&RttTracer));
  Config::ConnectWithoutContext (base + "CongestionWindow", MakeCallback (&CwndTracer));
  Config::ConnectWithoutContext (base + "CongState", MakeCallback (&CongStateTracer));
}

int main (int argc, char *argv[])
{
  CommandLine cmd (__FILE__);
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);

  NodeContainer nodes;
  nodes.Create (2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("1Gbps"));
  // Delay sampled once per run (seeded via RngRun) so std_sim isn't always
  // 0. Applied via MicroSeconds, not MilliSeconds: MilliSeconds(double)
  // truncates to whole ms, silently zeroing any sub-1ms value.
  Ptr<UniformRandomVariable> delayRv = CreateObject<UniformRandomVariable> ();
  delayRv->SetAttribute ("Min", DoubleValue (50.0));
  delayRv->SetAttribute ("Max", DoubleValue (150.0));
  pointToPoint.SetChannelAttribute ("Delay", TimeValue (MicroSeconds (delayRv->GetValue ())));

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

  Simulator::Schedule (Seconds (0.51), &ConnectTcpTraces, nodes.Get (1)->GetId ());

  Simulator::Stop (Seconds (6.0));
  Simulator::Run ();

  // Calculate throughput
  Ptr<PacketSink> sink = DynamicCast<PacketSink> (sinkApp.Get (0));
  uint64_t totalBytesReceived = sink->GetTotalRx ();
  double duration = 5.0; // Client ran for 5.5 - 0.5 = 5 seconds
  double throughputMbps = (totalBytesReceived * 8.0) / (duration * 1e6);

  double latencyMs = 0.0;
  double jitterMs = 0.0;
  if (!g_rttSamplesMs.empty ())
    {
      for (double d : g_rttSamplesMs) latencyMs += d;
      latencyMs /= g_rttSamplesMs.size ();
      if (g_rttSamplesMs.size () > 1)
        {
          double variance = 0.0;
          for (double d : g_rttSamplesMs) variance += (d - latencyMs) * (d - latencyMs);
          variance /= g_rttSamplesMs.size ();
          jitterMs = std::sqrt (variance);
        }
    }

  double cwndMeanKB = 0.0;
  if (!g_cwndSamplesKB.empty ())
    {
      for (double d : g_cwndSamplesKB) cwndMeanKB += d;
      cwndMeanKB /= g_cwndSamplesKB.size ();
    }

  std::cout << "NS3_METRIC throughput: " << throughputMbps << " Mbps" << std::endl;
  std::cout << "NS3_METRIC latency: " << latencyMs << " ms" << std::endl;
  std::cout << "NS3_METRIC jitter: " << jitterMs << " ms" << std::endl;
  std::cout << "NS3_METRIC retransmits: " << g_retransmitEvents << " count" << std::endl;
  std::cout << "NS3_METRIC snd_cwnd: " << cwndMeanKB << " KB" << std::endl;

  Simulator::Destroy ();
  return 0;
}
