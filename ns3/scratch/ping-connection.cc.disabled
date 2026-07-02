/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Implement ICMP in ns-3 node to answer the ping in client node
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/fd-net-device-module.h"
#include "ns3/ethernet-header.h"
#include "ns3/arp-header.h"
#include "ns3/ipv4-header.h"
#include "ns3/icmpv4.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("PingSimulationExample");

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
                    }
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
                    }
                }
            }
        }
    }
}

int
main (int argc, char *argv[])
{
  std::cout << "Simulation main started" << std::endl;
  CommandLine cmd (__FILE__);
  cmd.Parse (argc, argv);

  // Use RealTime Simulator Impl
  GlobalValue::Bind ("SimulatorImplementationType", StringValue ("ns3::RealtimeSimulatorImpl"));
  GlobalValue::Bind ("ChecksumEnabled", BooleanValue (true));

  NodeContainer nodes;
  nodes.Create (1);

  EmuFdNetDeviceHelper emu;
  emu.SetDeviceName ("nk0");

  NetDeviceContainer devices = emu.Install (nodes.Get (0));

  // Enable packet capture
  emu.EnablePcap ("ping-connection", devices.Get (0), true);

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper addresses;
  addresses.SetBase ("10.10.0.0", "255.255.255.0", "0.0.0.1"); // starts from 10.10.0.1
  Ipv4InterfaceContainer interfaces = addresses.Assign (devices);

  // Hook trace sources on the NetDevice to log events
  devices.Get (0)->TraceConnectWithoutContext ("MacTx", MakeCallback (&DeviceTx));
  devices.Get (0)->TraceConnectWithoutContext ("MacRx", MakeCallback (&DeviceRx));

  std::cout << "Starting simulation run..." << std::endl;
  Simulator::Stop (Seconds (600.0));
  Simulator::Run ();
  std::cout << "Simulation run ended" << std::endl;
  Simulator::Destroy ();
  std::cout << "Simulation destroyed" << std::endl;

  return 0;
}
