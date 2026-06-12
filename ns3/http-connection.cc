/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/**
 * \file
 * \ingroup fd-net-device
 * \brief Example simulation showing an HTTP connection over a Netkit link using EmuFdNetDevice.
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/fd-net-device-module.h"
#include "ns3/ipv4-l3-protocol.h"
#include "ns3/arp-cache.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("HttpConnectionExample");

/**
 * Callback function invoked when the TCP socket successfully connects.
 *
 * \param socket The socket which has connected.
 */
void
ConnectionSucceeded (Ptr<Socket> socket)
{
  NS_LOG_INFO ("TCP Connection Succeeded!");
  std::cout << "TCP Connection Succeeded!" << std::endl;

  std::string httpPost = "POST / HTTP/1.1\r\n"
                         "Host: 10.10.0.2:8080\r\n"
                         "Content-Type: text/plain\r\n"
                         "Content-Length: 11\r\n"
                         "Connection: close\r\n\r\n"
                         "hello world";

  Ptr<Packet> packet = Create<Packet> ((uint8_t *) httpPost.c_str (), httpPost.length ());
  socket->Send (packet);

  NS_LOG_INFO ("Sent HTTP POST packet: hello world");
  std::cout << "Sent HTTP POST packet: hello world" << std::endl;
}

/**
 * Callback function invoked when the TCP socket fails to connect.
 *
 * \param socket The socket which failed to connect.
 */
void
ConnectionFailed (Ptr<Socket> socket)
{
  NS_LOG_ERROR ("TCP Connection Failed!");
  std::cout << "TCP Connection Failed!" << std::endl;
}

/**
 * Print network interface status and ARP cache details for debugging.
 *
 * \param node The simulator node to query.
 */
void
PrintDebugInfo (Ptr<Node> node) //AI generated
{
  std::cout << "--- DEBUG INFO at " << Simulator::Now ().GetSeconds () << "s ---" << std::endl;
  Ptr<Ipv4L3Protocol> ipv4 = node->GetObject<Ipv4L3Protocol> ();
  std::cout << "Number of interfaces: " << ipv4->GetNInterfaces () << std::endl;
  for (uint32_t i = 0; i < ipv4->GetNInterfaces (); ++i)
    {
      Ptr<Ipv4Interface> iface = ipv4->GetInterface (i);
      std::cout << "Interface " << i << ": ";
      for (uint32_t j = 0; j < iface->GetNAddresses (); ++j)
        {
          std::cout << iface->GetAddress (j).GetLocal () << " ";
        }
      std::cout << " ARP cache is " << (iface->GetArpCache () ? "NOT null" : "null") << " pointer = " << iface->GetArpCache () << std::endl;
      if (iface->GetArpCache ())
        {
          Ptr<OutputStreamWrapper> osm = Create<OutputStreamWrapper> (&std::cout);
          iface->GetArpCache ()->PrintArpCache (osm);
        }
    }
  std::cout << "-----------------------------------" << std::endl;
}

int
main (int argc, char *argv[])
{
  std::cout << "Simulation main started" << std::endl;
  CommandLine cmd (__FILE__);
  cmd.Parse (argc, argv);

  // Use RealTime Simulatior Impl
  GlobalValue::Bind ("SimulatorImplementationType", StringValue ("ns3::RealtimeSimulatorImpl"));
  GlobalValue::Bind ("ChecksumEnabled", BooleanValue (true));

  NodeContainer nodes;
  nodes.Create (1);

  EmuFdNetDeviceHelper emu;
  emu.SetDeviceName ("nk1");

  NetDeviceContainer devices = emu.Install (nodes.Get (0));

  // Enable packet capture
  emu.EnablePcap ("http-connection", devices.Get (0), true);

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper addresses;
  addresses.SetBase ("10.10.0.0", "255.255.255.0", "10.10.0.1");
  Ipv4InterfaceContainer interfaces = addresses.Assign (devices);

  // Add static ARP entry for the client container IP 10.10.0.2
  // since the Netkit virtual interface runs in NOARP mode by default (or to guarantee direct delivery)
  // AI generated code
  Ptr<Ipv4L3Protocol> ipv4 = nodes.Get (0)->GetObject<Ipv4L3Protocol> ();
  Ptr<Ipv4Interface> ipIface = ipv4->GetInterface (1);
  Ptr<ArpCache> arp = ipIface->GetArpCache ();
  if (arp)
    {
      std::cout << "ARP Cache is NOT null" << std::endl;
      ArpCache::Entry *entry = arp->Add (Ipv4Address ("10.10.0.2"));
      if (entry)
        {
          entry->SetMacAddress (Mac48Address ("00:00:00:00:00:02"));
          entry->MarkPermanent ();
        }
    }

  // Set up TCP socket
  Ptr<Socket> socket = Socket::CreateSocket (nodes.Get (0), TcpSocketFactory::GetTypeId ());
  socket->SetConnectCallback (MakeCallback (&ConnectionSucceeded), MakeCallback (&ConnectionFailed));

  InetSocketAddress remote = InetSocketAddress (Ipv4Address ("10.10.0.2"), 8080);

  // Schedule the connection after 5 seconds to ensure netkit interfaces are ready
  std::cout << "Scheduling socket connection..." << std::endl;
  Simulator::Schedule (Seconds (4.9), &PrintDebugInfo, nodes.Get (0));
  Simulator::Schedule (Seconds (5.0), &Socket::Connect, socket, remote);

  std::cout << "Starting simulation run..." << std::endl;
  Simulator::Stop (Seconds (15.0));
  Simulator::Run ();
  std::cout << "Simulation run ended" << std::endl;
  Simulator::Destroy ();
  std::cout << "Simulation destroyed" << std::endl;

  return 0;
}
