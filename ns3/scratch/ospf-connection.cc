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
 * \brief Example simulation showing ARP messages over a Netkit link using EmuFdNetDevice.
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/fd-net-device-module.h"
#include "ns3/ipv4-l3-protocol.h"
#include "ns3/arp-l3-protocol.h"
#include "ns3/ethernet-header.h"
#include "ns3/arp-header.h"
#include "ns3/ipv4-header.h"
#include "ns3/icmpv4.h"
#include "ns3/ospf-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("ArpSimulationExample");

/**
 * Callback function when a packet is transmitted on the device.
 */
void
DeviceTx (Ptr<const Packet> packet)
{
  Ptr<Packet> p = packet->Copy ();
  EthernetHeader ethHeader;
  if (p->RemoveHeader (ethHeader))
    {
      if (ethHeader.GetLengthType () == 0x0806) // ARP
        {
          ArpHeader arpHeader;
          if (p->RemoveHeader (arpHeader))
            {
              if (arpHeader.IsRequest ())
                {
                  std::cout << "NS-3: Sent ARP Request" << std::endl;
                }
              else if (arpHeader.IsReply ())
                {
                  std::cout << "NS-3: Sent ARP Response" << std::endl;
                }
            }
        }
      else if (ethHeader.GetLengthType () == 0x0800) // IPv4
        {
          Ipv4Header ipHeader;
          if (p->RemoveHeader (ipHeader))
            {
              if (ipHeader.GetProtocol () == 1) // ICMP
                {
                  Icmpv4Header icmpHeader;
                  if (p->RemoveHeader (icmpHeader))
                    {
                      if (icmpHeader.GetType () == Icmpv4Header::ICMPV4_ECHO)
                        {
                          std::cout << "NS-3: Sent ICMP Echo Request" << std::endl;
                        }
                      else if (icmpHeader.GetType () == Icmpv4Header::ICMPV4_ECHO_REPLY)
                        {
                          std::cout << "NS-3: Sent ICMP Echo Reply" << std::endl;
                        }
                      else if (icmpHeader.GetType () == Icmpv4Header::ICMPV4_DEST_UNREACH)
                        {
                          std::cout << "NS-3: Sent ICMP Dest Unreachable" << std::endl;
                        }
                    }
                }
              else if (ipHeader.GetProtocol () == 89) // OSPF
                {
                  std::cout << "NS-3: Sent OSPF Packet" << std::endl;
                }
            }
        }
    }
}

/**
 * Callback function when a packet is received on the device.
 */
void
DeviceRx (Ptr<const Packet> packet)
{
  Ptr<Packet> p = packet->Copy ();
  EthernetHeader ethHeader;
  if (p->RemoveHeader (ethHeader))
    {
      if (ethHeader.GetLengthType () == 0x0806) // ARP
        {
          ArpHeader arpHeader;
          if (p->RemoveHeader (arpHeader))
            {
              if (arpHeader.IsRequest ())
                {
                  std::cout << "NS-3: Received ARP Request" << std::endl;
                }
              else if (arpHeader.IsReply ())
                {
                  std::cout << "NS-3: Received ARP Response" << std::endl;
                }
            }
        }
      else if (ethHeader.GetLengthType () == 0x0800) // IPv4
        {
          Ipv4Header ipHeader;
          if (p->RemoveHeader (ipHeader))
            {
              if (ipHeader.GetProtocol () == 1) // ICMP
                {
                  Icmpv4Header icmpHeader;
                  if (p->RemoveHeader (icmpHeader))
                    {
                      if (icmpHeader.GetType () == Icmpv4Header::ICMPV4_ECHO)
                        {
                          std::cout << "NS-3: Received ICMP Echo Request" << std::endl;
                        }
                      else if (icmpHeader.GetType () == Icmpv4Header::ICMPV4_ECHO_REPLY)
                        {
                          std::cout << "NS-3: Received ICMP Echo Reply" << std::endl;
                        }
                      else if (icmpHeader.GetType () == Icmpv4Header::ICMPV4_DEST_UNREACH)
                        {
                          std::cout << "NS-3: Received ICMP Dest Unreachable" << std::endl;
                        }
                    }
                }
              else if (ipHeader.GetProtocol () == 89) // OSPF
                {
                  std::cout << "NS-3: Received OSPF Packet" << std::endl;
                }
            }
        }
    }
}

/**
 * Helper function to send a packet.
 */
void
SendPacket (Ptr<Socket> socket, Ptr<Packet> packet)
{
  socket->Send (packet);
}

int
main (int argc, char *argv[])
{
  CommandLine cmd (__FILE__);
  cmd.Parse (argc, argv);

  // Use RealTime Simulator Impl
  GlobalValue::Bind ("SimulatorImplementationType", StringValue ("ns3::RealtimeSimulatorImpl"));
  GlobalValue::Bind ("ChecksumEnabled", BooleanValue (true));

  NodeContainer nodes;
  nodes.Create (1);

  EmuFdNetDeviceHelper emu;
  emu.SetDeviceName ("nk1");

  NetDeviceContainer devices = emu.Install (nodes.Get (0));

  // Enable packet capture
  emu.EnablePcap ("arp-connection", devices.Get (0), true);

  // Configure OSPF Routing Protocol defaults
  Config::SetDefault ("ns3::OspfRoutingProtocol::HelloInterval", UintegerValue (10));
  Config::SetDefault ("ns3::OspfRoutingProtocol::RouterDeadInterval", UintegerValue (30));

  InternetStackHelper stack;
  OspfHelper ospf;
  stack.SetRoutingHelper (ospf);
  stack.Install (nodes);

  Ipv4AddressHelper addresses;
  addresses.SetBase ("10.10.0.0", "255.255.255.0", "0.0.0.1");
  Ipv4InterfaceContainer interfaces = addresses.Assign (devices);

  // Hook trace sources on the NetDevice to log ARP events
  devices.Get (0)->TraceConnectWithoutContext ("MacTx", MakeCallback (&DeviceTx));
  devices.Get (0)->TraceConnectWithoutContext ("MacRx", MakeCallback (&DeviceRx));

  // Set up UDP socket to trigger ARP request
  Ptr<Socket> socket = Socket::CreateSocket (nodes.Get (0), UdpSocketFactory::GetTypeId ());
  InetSocketAddress remote = InetSocketAddress (Ipv4Address ("10.10.0.2"), 9); // discard port
  socket->Connect (remote);

  Ptr<Packet> packet = Create<Packet> ((uint8_t *) "arp-trigger", 11);

  // Schedule triggering packet after 5.0 seconds
  std::cout << "Scheduling ARP trigger packet..." << std::endl;
  Simulator::Schedule (Seconds (5.0), &SendPacket, socket, packet);

  std::cout << "Starting simulation run..." << std::endl;
  Simulator::Stop (Seconds (40.0));
  Simulator::Run ();
  std::cout << "Simulation run ended" << std::endl;
  Simulator::Destroy ();
  std::cout << "Simulation destroyed" << std::endl;

  return 0;
}
