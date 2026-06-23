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

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/fd-net-device-module.h"
#include "ns3/ipv4-l3-protocol.h"
#include "ns3/arp-l3-protocol.h"
#include "ns3/ethernet-header.h"
#include "ns3/arp-header.h"
#include "ns3/ipv4-header.h"
#include "ns3/sctp-helper.h"
#include "ns3/sctp.h"
#include <iostream>
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SctpSimulationExample");

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
              if (ipHeader.GetProtocol () == 132) // SCTP
                {
                  std::cout << "NS-3: Sent SCTP Packet" << std::endl;
                }
            }
        }
    }
}

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
              if (ipHeader.GetProtocol () == 132) // SCTP
                {
                  std::cout << "NS-3: Received SCTP Packet" << std::endl;
                }
            }
        }
    }
}

void
ServerRx (Ptr<Socket> socket)
{
  Ptr<SctpSocket> sctpSocket = DynamicCast<SctpSocket> (socket);
  if (sctpSocket)
    {
      Ptr<Packet> packet = Create<Packet> ();
      SctpRecvInfo recvInfo;
      int bytesRead;
      while ((bytesRead = sctpSocket->RecvMsg (packet, recvInfo)) > 0)
        {
          uint8_t *buffer = new uint8_t[bytesRead + 1];
          packet->CopyData (buffer, bytesRead);
          buffer[bytesRead] = '\0';
          std::cout << "NS-3 Server Received Message: \"" << buffer << "\"" << std::endl;
          delete[] buffer;

          // Send a response back to the client
          SctpSendInfo sendInfo;
          sendInfo.streamId = recvInfo.streamId;
          sendInfo.ppid = recvInfo.ppid;
          sendInfo.flags = 0;
          Ptr<Packet> reply = Create<Packet> ((const uint8_t *)"Hello from NS-3 Server via SCTP", 31);
          sctpSocket->SendMsg (reply, sendInfo);
          std::cout << "NS-3 Server Sent Response" << std::endl;
          packet = Create<Packet> (); // reset for next iteration
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
  emu.SetDeviceName ("nk1");

  NetDeviceContainer devices = emu.Install (nodes.Get (0));

  // Enable packet capture
  emu.EnablePcap ("sctp-connection", devices.Get (0), true);

  InternetStackHelper stack;
  stack.Install (nodes);

  // Install SCTP Stack
  SctpHelper sctp;
  sctp.Install (nodes);

  Ipv4AddressHelper addresses;
  addresses.SetBase ("10.10.0.0", "255.255.255.0", "0.0.0.1");
  Ipv4InterfaceContainer interfaces = addresses.Assign (devices);

  // Hook trace sources on the NetDevice
  devices.Get (0)->TraceConnectWithoutContext ("MacTx", MakeCallback (&DeviceTx));
  devices.Get (0)->TraceConnectWithoutContext ("MacRx", MakeCallback (&DeviceRx));

  // Create Server socket (bound to server addresses on port 5001)
  Ptr<Socket> serverSocket = Socket::CreateSocket (nodes.Get (0), SctpSocketFactory::GetTypeId ());
  Ptr<SctpSocket> sctpServer = DynamicCast<SctpSocket> (serverSocket);
  
  std::vector<Ipv4Address> serverAddrs;
  serverAddrs.push_back (Ipv4Address ("10.10.0.1"));
  
  sctpServer->Bindx (serverAddrs, 5001);
  sctpServer->Listen ();
  sctpServer->SetRecvCallback (MakeCallback (&ServerRx));

  std::cout << "SCTP Server listening on 10.10.0.1:5001..." << std::endl;

  std::cout << "Starting simulation run..." << std::endl;
  Simulator::Stop (Seconds (40.0));
  Simulator::Run ();
  std::cout << "Simulation run ended" << std::endl;

  serverSocket->Close ();
  Simulator::Destroy ();
  std::cout << "Simulation destroyed" << std::endl;

  return 0;
}
