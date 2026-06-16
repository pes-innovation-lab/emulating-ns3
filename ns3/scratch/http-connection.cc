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
 * Callback function invoked when the TCP socket receives a packet.
 *
 * \param socket The socket which received data.
 */
void
ReceivePacket (Ptr<Socket> socket)
{
  Ptr<Packet> packet;
  while ((packet = socket->Recv ()))
    {
      uint32_t size = packet->GetSize ();
      uint8_t *buffer = new uint8_t[size + 1];
      packet->CopyData (buffer, size);
      buffer[size] = '\0';
      std::cout << "Received response: " << buffer << std::endl;
      delete[] buffer;
    }
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
  addresses.SetBase ("10.10.0.0", "255.255.255.0", "0.0.0.1");
  Ipv4InterfaceContainer interfaces = addresses.Assign (devices);


  // Set up TCP socket
  Ptr<Socket> socket = Socket::CreateSocket (nodes.Get (0), TcpSocketFactory::GetTypeId ());
  socket->SetConnectCallback (MakeCallback (&ConnectionSucceeded), MakeCallback (&ConnectionFailed));
  socket->SetRecvCallback (MakeCallback (&ReceivePacket));

  InetSocketAddress remote = InetSocketAddress (Ipv4Address ("10.10.0.2"), 8080);

  // Schedule the connection after 5 seconds to ensure netkit interfaces are ready
  std::cout << "Scheduling socket connection..." << std::endl;
  Simulator::Schedule (Seconds (5.0), &Socket::Connect, socket, remote);

  std::cout << "Starting simulation run..." << std::endl;
  Simulator::Stop (Seconds (15.0));
  Simulator::Run ();
  std::cout << "Simulation run ended" << std::endl;
  Simulator::Destroy ();
  std::cout << "Simulation destroyed" << std::endl;

  return 0;
}
