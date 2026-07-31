#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/tcp-socket-base.h"

#include "../sim-common/env-config.h"

#include <cmath>
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SimTcpEvaluation");

static std::vector<double> g_rttSamplesMs;
static double g_cwndMaxKB = 0.0;
static uint32_t g_retransmitEvents = 0;

void
RttTracer (Time oldRtt, Time newRtt)
{
  g_rttSamplesMs.push_back (newRtt.GetSeconds () * 1000.0);
}

void
CwndTracer (uint32_t oldCwnd, uint32_t newCwnd)
{
  // Peak: matches iperf3's TCP_INFO max_snd_cwnd
  double kb = newCwnd / 1024.0;
  if (kb > g_cwndMaxKB)
    {
      g_cwndMaxKB = kb;
    }
}

void
CongStateTracer (TcpSocketState::TcpCongState_t oldState, TcpSocketState::TcpCongState_t newState)
{
  // ns-3 has no per-segment retransmit counter matching iperf3's TCP_INFO
  // retransmits; CA_LOSS/CA_RECOVERY entry is the socket detecting loss and
  // retransmitting, so counting those transitions is a retransmit-episode
  // proxy, not a literal segment count.
  if (newState == TcpSocketState::CA_LOSS || newState == TcpSocketState::CA_RECOVERY)
    {
      g_retransmitEvents++;
    }
}

void
ConnectTcpTraces (uint32_t nodeId)
{
  // BulkSend's socket doesn't exist until StartApplication runs, so connect
  // just after Start() - the wildcard path only matches existing sockets.
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

  // Matches ns-3 defaults unless config.toml's ns3_env sets these to the
  // real testbed's sysctl/socket values (cc algorithm, MSS, buffers, initcwnd).
  std::string tcpCc = GetEnvStr ("NS3_TCP_CC", "");
  if (!tcpCc.empty ())
    {
      Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue (tcpCc));
    }
  uint32_t mss = GetEnvUint ("NS3_TCP_SEGMENT_SIZE", 0);
  if (mss)
    {
      Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (mss));
    }
  uint32_t initCwnd = GetEnvUint ("NS3_TCP_INIT_CWND", 0);
  if (initCwnd)
    {
      Config::SetDefault ("ns3::TcpSocket::InitialCwnd", UintegerValue (initCwnd));
    }
  uint32_t sndBuf = GetEnvUint ("NS3_TCP_SND_BUF", 0);
  if (sndBuf)
    {
      Config::SetDefault ("ns3::TcpSocket::SndBufSize", UintegerValue (sndBuf));
    }
  uint32_t rcvBuf = GetEnvUint ("NS3_TCP_RCV_BUF", 0);
  if (rcvBuf)
    {
      Config::SetDefault ("ns3::TcpSocket::RcvBufSize", UintegerValue (rcvBuf));
    }

  double durationS = GetEnvDouble ("NS3_TCP_DURATION_S", 5.0);

  NodeContainer nodes;
  nodes.Create (2);

  PointToPointHelper pointToPoint;
  // Default: 1Gbps NIC-to-NIC macvlan run; override via NS3_DATA_RATE/
  // NS3_LINK_DELAY_US in config.toml if the real link's rating/delay is known.
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
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", sinkLocalAddress);
  ApplicationContainer sinkApp = packetSinkHelper.Install (nodes.Get (0));
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (durationS + 1.0));

  // Client: BulkSend on Node 1 (10.10.0.2) -> server (10.10.0.1)
  AddressValue remoteAddress (InetSocketAddress (interfaces.GetAddress (0), port));
  BulkSendHelper sourceHelper ("ns3::TcpSocketFactory", Address ());
  sourceHelper.SetAttribute ("Remote", remoteAddress);
  sourceHelper.SetAttribute ("MaxBytes", UintegerValue (0)); // Unlimited

  ApplicationContainer sourceApp = sourceHelper.Install (nodes.Get (1));
  sourceApp.Start (Seconds (0.5));
  sourceApp.Stop (Seconds (0.5 + durationS));

  Simulator::Schedule (Seconds (0.51), &ConnectTcpTraces, nodes.Get (1)->GetId ());

  Simulator::Stop (Seconds (durationS + 1.0));
  Simulator::Run ();

  Ptr<PacketSink> sink = DynamicCast<PacketSink> (sinkApp.Get (0));
  uint64_t totalBytesReceived = sink->GetTotalRx ();
  double throughputMbps = (totalBytesReceived * 8.0) / (durationS * 1e6);

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

  std::cout << "NS3_METRIC throughput: " << throughputMbps << " Mbps" << std::endl;
  std::cout << "NS3_METRIC latency: " << latencyMs << " ms" << std::endl;
  std::cout << "NS3_METRIC jitter: " << jitterMs << " ms" << std::endl;
  std::cout << "NS3_METRIC retransmits: " << g_retransmitEvents << " count" << std::endl;
  std::cout << "NS3_METRIC snd_cwnd: " << g_cwndMaxKB << " KB" << std::endl;

  Simulator::Destroy ();
  return 0;
}
