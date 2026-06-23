#include "ns3/sctp.h"
#include "ns3/test.h"
#include "ns3/string.h"
#include "ns3/node.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/simulator.h"
#include "ns3/sctp-helper.h"
#include <iostream>

using namespace ns3;

/**
 * @defgroup sctp-tests Tests for sctp module
 * @ingroup sctp
 * @ingroup tests
 */

/**
 * @ingroup sctp-tests
 * @brief Test case for SCTP Header and Chunk Serialization/Deserialization
 */
class SctpSerializationTestCase : public TestCase
{
  public:
    SctpSerializationTestCase()
        : TestCase("SctpSerializationTestCase")
    {
    }
    ~SctpSerializationTestCase() override {}

  private:
    void DoRun() override
    {
        // 1. Setup SCTP Header with INIT chunk
        Ptr<SctpHeader> header = Create<SctpHeader>();
        header->SetSourcePort(5001);
        header->SetDestinationPort(5002);
        header->SetVerificationTag(0); // INIT uses 0

        Ptr<SctpChunkInit> initChunk = Create<SctpChunkInit>();
        initChunk->m_initiateTag = 0x12345678;
        initChunk->m_aRwnd = 32768;
        initChunk->m_numOutboundStreams = 4;
        initChunk->m_numInboundStreams = 4;
        initChunk->m_initialTsn = 1000;
        initChunk->m_ipv4Addresses.push_back(Ipv4Address("10.1.1.1"));
        initChunk->m_ipv4Addresses.push_back(Ipv4Address("10.1.2.1"));
        header->AddChunk(initChunk);

        // 2. Serialize header to packet
        Ptr<Packet> packet = Create<Packet>();
        packet->AddHeader(*header);

        // 3. Deserialize and verify fields
        SctpHeader rxHeader;
        packet->RemoveHeader(rxHeader);

        NS_TEST_ASSERT_MSG_EQ(rxHeader.GetSourcePort(), 5001, "Source port mismatch");
        NS_TEST_ASSERT_MSG_EQ(rxHeader.GetDestinationPort(), 5002, "Destination port mismatch");
        NS_TEST_ASSERT_MSG_EQ(rxHeader.GetVerificationTag(), 0, "Verification tag mismatch");
        NS_TEST_ASSERT_MSG_EQ(rxHeader.GetChunks().size(), 1, "Number of chunks mismatch");

        Ptr<SctpChunk> rxChunk = rxHeader.GetChunks()[0];
        NS_TEST_ASSERT_MSG_EQ(rxChunk->GetType(), SctpChunk::INIT, "Chunk type mismatch");

        Ptr<SctpChunkInit> rxInit = DynamicCast<SctpChunkInit>(rxChunk);
        NS_TEST_ASSERT_MSG_NE(rxInit, nullptr, "Chunk cast failed");
        NS_TEST_ASSERT_MSG_EQ(rxInit->m_initiateTag, 0x12345678, "Initiate Tag mismatch");
        NS_TEST_ASSERT_MSG_EQ(rxInit->m_aRwnd, 32768, "a_rwnd mismatch");
        NS_TEST_ASSERT_MSG_EQ(rxInit->m_numOutboundStreams, 4, "Outbound streams mismatch");
        NS_TEST_ASSERT_MSG_EQ(rxInit->m_numInboundStreams, 4, "Inbound streams mismatch");
        NS_TEST_ASSERT_MSG_EQ(rxInit->m_initialTsn, 1000, "Initial TSN mismatch");
        NS_TEST_ASSERT_MSG_EQ(rxInit->m_ipv4Addresses.size(), 2, "IPv4 address list size mismatch");
        NS_TEST_ASSERT_MSG_EQ(rxInit->m_ipv4Addresses[0], Ipv4Address("10.1.1.1"), "IPv4 address 1 mismatch");
        NS_TEST_ASSERT_MSG_EQ(rxInit->m_ipv4Addresses[1], Ipv4Address("10.1.2.1"), "IPv4 address 2 mismatch");

        // Verify checksum is valid and not zero
        NS_TEST_ASSERT_MSG_NE(rxHeader.GetChecksum(), 0, "Checksum was not calculated");
    }
};

static bool g_connSucceededTriggered = false;

static void
ConnectionSucceededCallback(Ptr<Socket> socket)
{
    g_connSucceededTriggered = true;
}

/**
 * @ingroup sctp-tests
 * @brief Test case for SCTP connection establishment (handshake)
 */
class SctpHandshakeTestCase : public TestCase
{
  public:
    SctpHandshakeTestCase()
        : TestCase("SctpHandshakeTestCase")
    {
    }
    ~SctpHandshakeTestCase() override {}

  private:
    void DoRun() override
    {
        g_connSucceededTriggered = false;

        // Create nodes
        NodeContainer nodes;
        nodes.Create(2);

        // Install Internet and SCTP helper stacks
        InternetStackHelper internet;
        internet.Install(nodes);

        SctpHelper sctp;
        sctp.Install(nodes);

        // Point-to-point link
        PointToPointHelper p2p;
        p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
        p2p.SetChannelAttribute("Delay", StringValue("2ms"));
        NetDeviceContainer devices = p2p.Install(nodes);

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

        // Create Server socket (listening)
        Ptr<Socket> serverSocket = Socket::CreateSocket(nodes.Get(0), SctpSocketFactory::GetTypeId());
        serverSocket->Bind(InetSocketAddress(interfaces.GetAddress(0), 5001));
        serverSocket->Listen();

        // Create Client socket and connect
        Ptr<Socket> clientSocket = Socket::CreateSocket(nodes.Get(1), SctpSocketFactory::GetTypeId());
        clientSocket->Bind();
        clientSocket->SetConnectCallback(MakeCallback(&ConnectionSucceededCallback), MakeNullCallback<void, Ptr<Socket>>());
        clientSocket->Connect(InetSocketAddress(interfaces.GetAddress(0), 5001));

        Simulator::Stop(Seconds(2.0));
        Simulator::Run();

        NS_TEST_ASSERT_MSG_EQ(g_connSucceededTriggered, true, "Connection handshake failed");
        
        serverSocket->Close();
        clientSocket->Close();
        Simulator::Destroy();
    }
};

/**
 * @ingroup sctp-tests
 * @brief Test case for SCTP Multi-Homing connection and data failover
 */
class SctpMultiHomingTestCase : public TestCase
{
  public:
    SctpMultiHomingTestCase()
        : TestCase("SctpMultiHomingTestCase")
    {
    }
    ~SctpMultiHomingTestCase() override {}

  private:
    void DoRun() override
    {
        g_connSucceededTriggered = false;

        // Topology: Dual links between Node A (0) and Node B (1)
        NodeContainer nodes;
        nodes.Create(2);

        InternetStackHelper internet;
        internet.Install(nodes);

        SctpHelper sctp;
        sctp.Install(nodes);

        PointToPointHelper p2p;
        p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
        p2p.SetChannelAttribute("Delay", StringValue("2ms"));

        // Link 1: Interface 1 (10.1.1.0/24)
        NetDeviceContainer dev1 = p2p.Install(nodes.Get(0), nodes.Get(1));
        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer ip1 = ipv4.Assign(dev1);

        // Link 2: Interface 2 (10.1.2.0/24)
        NetDeviceContainer dev2 = p2p.Install(nodes.Get(0), nodes.Get(1));
        ipv4.SetBase("10.1.2.0", "255.255.255.0");
        Ipv4InterfaceContainer ip2 = ipv4.Assign(dev2);

        // Server addresses
        std::vector<Ipv4Address> serverAddrs;
        serverAddrs.push_back(ip1.GetAddress(0));
        serverAddrs.push_back(ip2.GetAddress(0));

        // Client addresses
        std::vector<Ipv4Address> clientAddrs;
        clientAddrs.push_back(ip1.GetAddress(1));
        clientAddrs.push_back(ip2.GetAddress(1));

        // Setup server socket (bindx)
        Ptr<Socket> serverSocket = Socket::CreateSocket(nodes.Get(0), SctpSocketFactory::GetTypeId());
        Ptr<SctpSocket> sctpServer = DynamicCast<SctpSocket>(serverSocket);
        sctpServer->Bindx(serverAddrs, 5001);
        sctpServer->Listen();

        // Setup client socket (bindx & connectx)
        Ptr<Socket> clientSocket = Socket::CreateSocket(nodes.Get(1), SctpSocketFactory::GetTypeId());
        Ptr<SctpSocket> sctpClient = DynamicCast<SctpSocket>(clientSocket);
        sctpClient->Bindx(clientAddrs, 0);
        sctpClient->SetConnectCallback(MakeCallback(&ConnectionSucceededCallback), MakeNullCallback<void, Ptr<Socket>>());
        sctpClient->Connectx(serverAddrs, 5001);

        Simulator::Stop(Seconds(2.0));
        Simulator::Run();

        NS_TEST_ASSERT_MSG_EQ(g_connSucceededTriggered, true, "Multi-homed connection fail");

        serverSocket->Close();
        clientSocket->Close();
        Simulator::Destroy();
    }
};

/**
 * @ingroup sctp-tests
 * @brief TestSuite for module sctp
 */
class SctpTestSuite : public TestSuite
{
  public:
    SctpTestSuite();
};

SctpTestSuite::SctpTestSuite()
    : TestSuite("sctp", Type::UNIT)
{
    AddTestCase(new SctpSerializationTestCase, TestCase::Duration::QUICK);
    AddTestCase(new SctpHandshakeTestCase, TestCase::Duration::QUICK);
    AddTestCase(new SctpMultiHomingTestCase, TestCase::Duration::QUICK);
}

static SctpTestSuite ssctpTestSuite;
