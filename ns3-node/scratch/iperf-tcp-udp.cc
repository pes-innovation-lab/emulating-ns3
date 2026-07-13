/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright 2026 PES Innovation Lab
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/fd-net-device-module.h"
#include "ns3/applications-module.h"
#include <iostream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("IPerfSimulationExample");

uint64_t g_lastTotalRx = 0;

void
PrintThroughput (Ptr<PacketSink> sink)
{
  uint64_t totalRx = sink->GetTotalRx ();
  uint64_t diff = totalRx - g_lastTotalRx;
  g_lastTotalRx = totalRx;
  double throughputMps = (diff * 8.0) / (1.0 * 1000000.0); // Mbps
  std::cout << "NS-3 Receiver: Time " << Simulator::Now ().GetSeconds ()
            << "s - Total Received: " << totalRx << " bytes, Throughput: "
            << throughputMps << " Mbps" << std::endl;
  Simulator::Schedule (Seconds (1.0), &PrintThroughput, sink);
}

void
RxCallback (Ptr<const Packet> packet, const Address &address)
{
  NS_LOG_DEBUG ("Received packet of size " << packet->GetSize () << " from " << address);
}

int
main (int argc, char *argv[])
{
  std::string protocol = "tcp";
  uint16_t port = 5001; // default iperf port

  CommandLine cmd (__FILE__);
  cmd.AddValue ("protocol", "Transport protocol to test: tcp or udp (default tcp)", protocol);
  cmd.AddValue ("port", "Port to listen on (default 5001)", port);
  cmd.Parse (argc, argv);

  std::cout << "Simulation main started (protocol=" << protocol << ", port=" << port << ")" << std::endl;

  // Bind to RealTime Simulator for emulation
  GlobalValue::Bind ("SimulatorImplementationType", StringValue ("ns3::RealtimeSimulatorImpl"));
  GlobalValue::Bind ("ChecksumEnabled", BooleanValue (true));

  NodeContainer nodes;
  nodes.Create (1);

  EmuFdNetDeviceHelper emu;
  emu.SetDeviceName ("nk0");

  NetDeviceContainer devices = emu.Install (nodes.Get (0));

  // Enable packet capture on nk0
  emu.EnablePcap ("iperf-tcp-udp", devices.Get (0), true);

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper addresses;
  addresses.SetBase ("10.10.0.0", "255.255.255.0", "0.0.0.1"); // sets IP 10.10.0.1 on the device
  Ipv4InterfaceContainer interfaces = addresses.Assign (devices);

  std::cout << "NS-3 simulated node IP: " << interfaces.GetAddress (0) << std::endl;

  // Set up PacketSink
  Address sinkAddress;
  std::string socketFactory;

  if (protocol == "udp" || protocol == "UDP")
    {
      sinkAddress = InetSocketAddress (Ipv4Address::GetAny (), port);
      socketFactory = "ns3::UdpSocketFactory";
    }
  else
    {
      sinkAddress = InetSocketAddress (Ipv4Address::GetAny (), port);
      socketFactory = "ns3::TcpSocketFactory";
    }

  PacketSinkHelper packetSinkHelper (socketFactory, sinkAddress);
  ApplicationContainer sinkApp = packetSinkHelper.Install (nodes.Get (0));
  
  Ptr<PacketSink> sink = sinkApp.Get (0)->GetObject<PacketSink> ();
  sink->TraceConnectWithoutContext ("Rx", MakeCallback (&RxCallback));

  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (600.0));

  // Schedule throughput print every 1.0 second
  Simulator::Schedule (Seconds (1.0), &PrintThroughput, sink);

  std::cout << "Starting simulation run..." << std::endl;
  Simulator::Stop (Seconds (600.0));
  Simulator::Run ();
  std::cout << "Simulation run ended" << std::endl;
  Simulator::Destroy ();
  std::cout << "Simulation destroyed" << std::endl;

  return 0;
}
