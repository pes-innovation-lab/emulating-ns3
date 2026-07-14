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
#include "ns3/ipv4-l3-protocol.h"
#include "ns3/ethernet-header.h"
#include "ns3/arp-header.h"
#include "ns3/ipv4-header.h"
#include "ns3/udp-header.h"
#include "ns3/icmpv4.h"
#include "ns3/internet-apps-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("DhcpSimulationExample");

/**
 * Print DHCP message type as string.
 */
std::string
GetDhcpTypeStr (uint8_t type)
{
  if (type == DhcpHeader::DHCPDISCOVER) return "DHCP Discover";
  if (type == DhcpHeader::DHCPOFFER) return "DHCP Offer";
  if (type == DhcpHeader::DHCPREQ) return "DHCP Request";
  if (type == DhcpHeader::DHCPACK) return "DHCP Ack";
  if (type == DhcpHeader::DHCPNACK) return "DHCP Nack";
  return "Unknown";
}

/**
 * Log decoded details of transmitted packets.
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
              else if (ipHeader.GetProtocol () == 17) // UDP
                {
                  UdpHeader udpHeader;
                  if (p->RemoveHeader (udpHeader))
                    {
                      if (udpHeader.GetSourcePort () == 67 || udpHeader.GetDestinationPort () == 67 ||
                          udpHeader.GetSourcePort () == 68 || udpHeader.GetDestinationPort () == 68)
                        {
                          DhcpHeader dhcpHeader;
                          if (p->RemoveHeader (dhcpHeader))
                            {
                              std::cout << "NS-3: Sent DHCP Packet (" << GetDhcpTypeStr (dhcpHeader.GetType ()) << ")" << std::endl;
                            }
                        }
                    }
                }
            }
        }
    }
}

/**
 * Log decoded details of received packets.
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
              else if (ipHeader.GetProtocol () == 17) // UDP
                {
                  UdpHeader udpHeader;
                  if (p->RemoveHeader (udpHeader))
                    {
                      if (udpHeader.GetSourcePort () == 67 || udpHeader.GetDestinationPort () == 67 ||
                          udpHeader.GetSourcePort () == 68 || udpHeader.GetDestinationPort () == 68)
                        {
                          DhcpHeader dhcpHeader;
                          if (p->RemoveHeader (dhcpHeader))
                            {
                              std::cout << "NS-3: Received DHCP Packet (" << GetDhcpTypeStr (dhcpHeader.GetType ()) << ")" << std::endl;
                            }
                        }
                    }
                }
            }
        }
    }
}

int
main (int argc, char *argv[])
{
  CommandLine cmd (__FILE__);
  cmd.Parse (argc, argv);

  // Bind to RealTime Simulator for emulation
  GlobalValue::Bind ("SimulatorImplementationType", StringValue ("ns3::RealtimeSimulatorImpl"));
  GlobalValue::Bind ("ChecksumEnabled", BooleanValue (true));

  NodeContainer nodes;
  nodes.Create (1);

  EmuFdNetDeviceHelper emu;
  emu.SetDeviceName ("nk0");

  NetDeviceContainer devices = emu.Install (nodes.Get (0));

  // Enable packet capture on nk0
  emu.EnablePcap ("dhcp-connection", devices.Get (0), true);

  InternetStackHelper stack;
  stack.Install (nodes);

  NS_LOG_INFO ("Configuring DHCP Server...");
  DhcpHelper dhcpHelper;
  ApplicationContainer dhcpServerApp = dhcpHelper.InstallDhcpServer (devices.Get (0),
                                                                    Ipv4Address ("10.10.0.1"),
                                                                    Ipv4Address ("10.10.0.0"),
                                                                    Ipv4Mask ("255.255.255.0"),
                                                                    Ipv4Address ("10.10.0.10"),
                                                                    Ipv4Address ("10.10.0.100"),
                                                                    Ipv4Address ("10.10.0.1"));

  // Connect trace callbacks on the netdevice
  devices.Get (0)->TraceConnectWithoutContext ("MacTx", MakeCallback (&DeviceTx));
  devices.Get (0)->TraceConnectWithoutContext ("MacRx", MakeCallback (&DeviceRx));

  // Start/stop DHCP application
  dhcpServerApp.Start (Seconds (0.0));
  dhcpServerApp.Stop (Seconds (600.0));

  std::cout << "Starting DHCP server simulation..." << std::endl;
  Simulator::Stop (Seconds (600.0));
  Simulator::Run ();
  std::cout << "Simulation run ended" << std::endl;
  Simulator::Destroy ();
  std::cout << "Simulation destroyed" << std::endl;

  return 0;
}
