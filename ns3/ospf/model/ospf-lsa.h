#ifndef OSPF_LSA_H
#define OSPF_LSA_H

#include "ns3/ipv4-address.h"
#include "ns3/buffer.h"
#include <vector>
#include <iostream>

namespace ns3 {

/**
 * \ingroup ospf
 * \brief OSPF LSA Header (20 bytes) as defined in RFC 2328 Section A.4.1.
 */
struct OspfLsaHeader
{
  uint16_t m_lsAge; ///< Link State Age in seconds
  uint8_t m_options; ///< OSPF Options
  uint8_t m_lsType; ///< LSA Type (1 = Router LSA, 2 = Network LSA)
  Ipv4Address m_linkStateId; ///< Link State ID
  Ipv4Address m_advertisingRouter; ///< Router ID of advertising router
  uint32_t m_lsSequenceNumber; ///< LSA Sequence Number
  uint16_t m_lsChecksum; ///< LSA Checksum
  uint16_t m_length; ///< Total length of LSA including header

  OspfLsaHeader();

  /**
   * Serialize LSA Header to a buffer iterator.
   * @param start Buffer iterator to write to
   */
  void Serialize(Buffer::Iterator& start) const;

  /**
   * Deserialize LSA Header from a buffer iterator.
   * @param start Buffer iterator to read from
   */
  void Deserialize(Buffer::Iterator& start);

  /**
   * Print LSA Header fields.
   * @param os Output stream
   */
  void Print(std::ostream &os) const;
};

/**
 * \ingroup ospf
 * \brief OSPF Link Record inside Router LSA.
 */
struct OspfLinkRecord
{
  Ipv4Address m_linkId; ///< Link ID
  Ipv4Address m_linkData; ///< Link Data
  uint8_t m_type; ///< Link Type: 1 (Point-to-Point), 2 (Transit), 3 (Stub), 4 (Virtual)
  uint8_t m_numTos; ///< Number of TOS metrics (always 0)
  uint16_t m_metric; ///< Link Metric (Cost)

  OspfLinkRecord();

  /**
   * Serialize Link Record to a buffer iterator.
   * @param start Buffer iterator to write to
   */
  void Serialize(Buffer::Iterator& start) const;

  /**
   * Deserialize Link Record from a buffer iterator.
   * @param start Buffer iterator to read from
   */
  void Deserialize(Buffer::Iterator& start);
};

/**
 * \ingroup ospf
 * \brief Router LSA (Type 1 LSA) representing router connections.
 */
struct OspfRouterLsa
{
  OspfLsaHeader m_header; ///< LSA Common Header
  uint8_t m_flags; ///< Flags: B, E, V
  uint8_t m_zero; ///< Must be zero
  std::vector<OspfLinkRecord> m_links; ///< List of link records

  OspfRouterLsa();

  /**
   * Serialize Router LSA.
   * @param start Buffer iterator to write to
   */
  void Serialize(Buffer::Iterator& start) const;

  /**
   * Deserialize Router LSA.
   * @param start Buffer iterator to read from
   */
  void Deserialize(Buffer::Iterator& start);

  /**
   * Print LSA details.
   * @param os Output stream
   */
  void Print(std::ostream &os) const;
};

/**
 * \ingroup ospf
 * \brief Network LSA (Type 2 LSA) representing transit multi-access networks.
 */
struct OspfNetworkLsa
{
  OspfLsaHeader m_header; ///< LSA Common Header
  Ipv4Address m_networkMask; ///< Network Mask of the transit network
  std::vector<Ipv4Address> m_attachedRouters; ///< List of attached Router IDs

  OspfNetworkLsa();

  /**
   * Serialize Network LSA.
   * @param start Buffer iterator to write to
   */
  void Serialize(Buffer::Iterator& start) const;

  /**
   * Deserialize Network LSA.
   * @param start Buffer iterator to read from
   */
  void Deserialize(Buffer::Iterator& start);

  /**
   * Print LSA details.
   * @param os Output stream
   */
  void Print(std::ostream &os) const;
};

} // namespace ns3

#endif // OSPF_LSA_H
