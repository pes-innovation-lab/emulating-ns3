#include "ns3/ospf-header.h"
#include "ns3/ospf-lsa.h"
#include "ns3/test.h"
#include "ns3/packet.h"

using namespace ns3;

/**
 * \defgroup ospf-tests Tests for ospf
 * \ingroup ospf
 * \ingroup tests
 */

/**
 * \ingroup ospf-tests
 * \brief Test case for OSPF Common Header serialization.
 */
class OspfHeaderTestCase : public TestCase
{
public:
  OspfHeaderTestCase();
  virtual ~OspfHeaderTestCase() override;

private:
  virtual void DoRun() override;
};

OspfHeaderTestCase::OspfHeaderTestCase()
  : TestCase("OSPF common header serialization test")
{}

OspfHeaderTestCase::~OspfHeaderTestCase()
{}

void
OspfHeaderTestCase::DoRun()
{
  OspfHeader header;
  header.SetType(1);
  header.SetPacketLength(100);
  header.SetRouterId(Ipv4Address("192.168.1.1"));
  header.SetAreaId(Ipv4Address("0.0.0.0"));

  Ptr<Packet> packet = Create<Packet>();
  packet->AddHeader(header);

  OspfHeader decoded;
  uint32_t read = packet->RemoveHeader(decoded);

  NS_TEST_ASSERT_MSG_EQ(read, 24, "Common header size should be 24 bytes");
  NS_TEST_ASSERT_MSG_EQ(decoded.GetType(), 1, "OSPF type field mismatch");
  NS_TEST_ASSERT_MSG_EQ(decoded.GetPacketLength(), 100, "OSPF length field mismatch");
  NS_TEST_ASSERT_MSG_EQ(decoded.GetRouterId(), Ipv4Address("192.168.1.1"), "OSPF router ID mismatch");
  NS_TEST_ASSERT_MSG_EQ(decoded.GetAreaId(), Ipv4Address("0.0.0.0"), "OSPF area ID mismatch");
}


/**
 * \ingroup ospf-tests
 * \brief Test case for OSPF Hello Header serialization.
 */
class OspfHelloTestCase : public TestCase
{
public:
  OspfHelloTestCase();
  virtual ~OspfHelloTestCase() override;

private:
  virtual void DoRun() override;
};

OspfHelloTestCase::OspfHelloTestCase()
  : TestCase("OSPF Hello header serialization test")
{}

OspfHelloTestCase::~OspfHelloTestCase()
{}

void
OspfHelloTestCase::DoRun()
{
  OspfHelloHeader hello;
  hello.SetNetworkMask(Ipv4Address("255.255.255.0"));
  hello.SetHelloInterval(10);
  hello.SetRouterPriority(2);
  hello.SetRouterDeadInterval(40);
  hello.SetDesignatedRouter(Ipv4Address("192.168.1.1"));
  hello.SetBackupDesignatedRouter(Ipv4Address("192.168.1.2"));
  hello.AddNeighbor(Ipv4Address("192.168.1.3"));
  hello.AddNeighbor(Ipv4Address("192.168.1.4"));

  Ptr<Packet> packet = Create<Packet>();
  packet->AddHeader(hello);

  OspfHelloHeader decoded;
  uint32_t read = packet->RemoveHeader(decoded);

  NS_TEST_ASSERT_MSG_EQ(read, 28, "Hello header size should be 28 bytes with 2 neighbors");
  NS_TEST_ASSERT_MSG_EQ(decoded.GetNetworkMask(), Ipv4Address("255.255.255.0"), "Hello network mask mismatch");
  NS_TEST_ASSERT_MSG_EQ(decoded.GetHelloInterval(), 10, "Hello interval mismatch");
  NS_TEST_ASSERT_MSG_EQ(decoded.GetRouterPriority(), 2, "Hello priority mismatch");
  NS_TEST_ASSERT_MSG_EQ(decoded.GetRouterDeadInterval(), 40, "Hello dead interval mismatch");
  NS_TEST_ASSERT_MSG_EQ(decoded.GetDesignatedRouter(), Ipv4Address("192.168.1.1"), "Hello DR mismatch");
  NS_TEST_ASSERT_MSG_EQ(decoded.GetBackupDesignatedRouter(), Ipv4Address("192.168.1.2"), "Hello BDR mismatch");
  
  const std::vector<Ipv4Address>& neighbors = decoded.GetNeighbors();
  NS_TEST_ASSERT_MSG_EQ(neighbors.size(), 2, "Hello neighbors count mismatch");
  NS_TEST_ASSERT_MSG_EQ(neighbors[0], Ipv4Address("192.168.1.3"), "Hello neighbor 1 mismatch");
  NS_TEST_ASSERT_MSG_EQ(neighbors[1], Ipv4Address("192.168.1.4"), "Hello neighbor 2 mismatch");
}


/**
 * \ingroup ospf-tests
 * \brief Test case for OSPF Link State Database (LSDB) updates and LSA serialization.
 */
class OspfLsaTestCase : public TestCase
{
public:
  OspfLsaTestCase();
  virtual ~OspfLsaTestCase() override;

private:
  virtual void DoRun() override;
};

OspfLsaTestCase::OspfLsaTestCase()
  : TestCase("OSPF LSA and LSU packet serialization test")
{}

OspfLsaTestCase::~OspfLsaTestCase()
{}

void
OspfLsaTestCase::DoRun()
{
  // 1. Create a Router LSA
  OspfRouterLsa rLsa;
  rLsa.m_header.m_lsAge = 1;
  rLsa.m_header.m_linkStateId = Ipv4Address("1.1.1.1");
  rLsa.m_header.m_advertisingRouter = Ipv4Address("1.1.1.1");
  rLsa.m_header.m_lsSequenceNumber = 0x80000001;
  rLsa.m_flags = 0x01; // B-flag

  OspfLinkRecord link;
  link.m_linkId = Ipv4Address("192.168.1.2");
  link.m_linkData = Ipv4Address("192.168.1.1");
  link.m_type = 1; // Point-to-point
  link.m_metric = 10;
  rLsa.m_links.push_back(link);

  // 2. Create a Network LSA
  OspfNetworkLsa nLsa;
  nLsa.m_header.m_lsAge = 2;
  nLsa.m_header.m_linkStateId = Ipv4Address("192.168.1.254");
  nLsa.m_header.m_advertisingRouter = Ipv4Address("2.2.2.2");
  nLsa.m_header.m_lsSequenceNumber = 0x80000002;
  nLsa.m_networkMask = Ipv4Address("255.255.255.0");
  nLsa.m_attachedRouters.push_back(Ipv4Address("1.1.1.1"));
  nLsa.m_attachedRouters.push_back(Ipv4Address("2.2.2.2"));

  // 3. Put them in an LSU packet
  OspfLinkStateUpdateHeader lsu;
  lsu.AddRouterLsa(rLsa);
  lsu.AddNetworkLsa(nLsa);

  Ptr<Packet> packet = Create<Packet>();
  packet->AddHeader(lsu);

  // 4. Decode the LSU packet
  OspfLinkStateUpdateHeader decodedLsu;
  packet->RemoveHeader(decodedLsu);

  // 5. Assert Router LSA correctness
  const std::vector<OspfRouterLsa>& rLsas = decodedLsu.GetRouterLsas();
  NS_TEST_ASSERT_MSG_EQ(rLsas.size(), 1, "Decoded router LSAs count mismatch");
  NS_TEST_ASSERT_MSG_EQ(rLsas[0].m_header.m_lsAge, 1, "Router LSA age mismatch");
  NS_TEST_ASSERT_MSG_EQ(rLsas[0].m_header.m_linkStateId, Ipv4Address("1.1.1.1"), "Router LSA LSID mismatch");
  NS_TEST_ASSERT_MSG_EQ(rLsas[0].m_header.m_advertisingRouter, Ipv4Address("1.1.1.1"), "Router LSA adv router mismatch");
  NS_TEST_ASSERT_MSG_EQ(rLsas[0].m_header.m_lsSequenceNumber, 0x80000001, "Router LSA sequence mismatch");
  NS_TEST_ASSERT_MSG_EQ(rLsas[0].m_flags, 0x01, "Router LSA flags mismatch");
  NS_TEST_ASSERT_MSG_EQ(rLsas[0].m_links.size(), 1, "Router LSA links count mismatch");
  NS_TEST_ASSERT_MSG_EQ(rLsas[0].m_links[0].m_linkId, Ipv4Address("192.168.1.2"), "Router LSA link ID mismatch");
  NS_TEST_ASSERT_MSG_EQ(rLsas[0].m_links[0].m_linkData, Ipv4Address("192.168.1.1"), "Router LSA link data mismatch");
  NS_TEST_ASSERT_MSG_EQ(rLsas[0].m_links[0].m_type, 1, "Router LSA link type mismatch");
  NS_TEST_ASSERT_MSG_EQ(rLsas[0].m_links[0].m_metric, 10, "Router LSA link metric mismatch");

  // 6. Assert Network LSA correctness
  const std::vector<OspfNetworkLsa>& nLsas = decodedLsu.GetNetworkLsas();
  NS_TEST_ASSERT_MSG_EQ(nLsas.size(), 1, "Decoded network LSAs count mismatch");
  NS_TEST_ASSERT_MSG_EQ(nLsas[0].m_header.m_lsAge, 2, "Network LSA age mismatch");
  NS_TEST_ASSERT_MSG_EQ(nLsas[0].m_header.m_linkStateId, Ipv4Address("192.168.1.254"), "Network LSA LSID mismatch");
  NS_TEST_ASSERT_MSG_EQ(nLsas[0].m_header.m_advertisingRouter, Ipv4Address("2.2.2.2"), "Network LSA adv router mismatch");
  NS_TEST_ASSERT_MSG_EQ(nLsas[0].m_header.m_lsSequenceNumber, 0x80000002, "Network LSA sequence mismatch");
  NS_TEST_ASSERT_MSG_EQ(nLsas[0].m_networkMask, Ipv4Address("255.255.255.0"), "Network LSA mask mismatch");
  NS_TEST_ASSERT_MSG_EQ(nLsas[0].m_attachedRouters.size(), 2, "Network LSA attached count mismatch");
  NS_TEST_ASSERT_MSG_EQ(nLsas[0].m_attachedRouters[0], Ipv4Address("1.1.1.1"), "Network LSA attached 1 mismatch");
  NS_TEST_ASSERT_MSG_EQ(nLsas[0].m_attachedRouters[1], Ipv4Address("2.2.2.2"), "Network LSA attached 2 mismatch");
}


/**
 * \ingroup ospf-tests
 * \brief OSPF Test Suite configuration.
 */
class OspfTestSuite : public TestSuite
{
public:
  OspfTestSuite();
};

OspfTestSuite::OspfTestSuite()
  : TestSuite("ospf", Type::UNIT)
{
  AddTestCase(new OspfHeaderTestCase, TestCase::Duration::QUICK);
  AddTestCase(new OspfHelloTestCase, TestCase::Duration::QUICK);
  AddTestCase(new OspfLsaTestCase, TestCase::Duration::QUICK);
}

static OspfTestSuite sospfTestSuite;
