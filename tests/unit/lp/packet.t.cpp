/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2013-2018 Regents of the University of California.
 *
 * This file is part of ndn-cxx library (NDN C++ library with eXperimental eXtensions).
 *
 * ndn-cxx library is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Lesser General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later version.
 *
 * ndn-cxx library is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more details.
 *
 * You should have received copies of the GNU General Public License and GNU Lesser
 * General Public License along with ndn-cxx, e.g., in COPYING.md file.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * See AUTHORS.md for complete list of ndn-cxx authors and contributors.
 */

#include "ndn-cxx/lp/packet.hpp"
#include "ndn-cxx/prefix-announcement.hpp"
#include "ndn-cxx/security/signature-sha256-with-rsa.hpp"

#include "tests/boost-test.hpp"
#include "tests/identity-management-fixture.hpp"

namespace ndn {
namespace lp {
namespace tests {

BOOST_AUTO_TEST_SUITE(Lp)
BOOST_AUTO_TEST_SUITE(TestPacket)

BOOST_AUTO_TEST_CASE(FieldAccess)
{
  Packet packet;

  BOOST_CHECK(packet.empty());
  BOOST_CHECK(!packet.has<FragIndexField>());
  BOOST_CHECK_EQUAL(0, packet.count<FragIndexField>());

  packet.set<FragIndexField>(1234);
  BOOST_CHECK(!packet.empty());
  BOOST_CHECK(packet.has<FragIndexField>());
  BOOST_CHECK_THROW(packet.add<FragIndexField>(5678), std::length_error);
  BOOST_CHECK_EQUAL(1, packet.count<FragIndexField>());
  BOOST_CHECK_EQUAL(1234, packet.get<FragIndexField>(0));
  BOOST_CHECK_THROW(packet.get<FragIndexField>(1), std::out_of_range);
  BOOST_CHECK_THROW(packet.remove<FragIndexField>(1), std::out_of_range);

  packet.remove<FragIndexField>(0);
  BOOST_CHECK_EQUAL(0, packet.count<FragIndexField>());

  packet.add<FragIndexField>(832);
  std::vector<uint64_t> fragIndexes = packet.list<FragIndexField>();
  BOOST_CHECK_EQUAL(1, fragIndexes.size());
  BOOST_CHECK_EQUAL(832, fragIndexes.at(0));

  packet.clear<FragIndexField>();
  BOOST_CHECK_EQUAL(0, packet.count<FragIndexField>());
  BOOST_CHECK(packet.empty());
}

/// \todo test field access methods with a REPEATABLE field

BOOST_AUTO_TEST_CASE(EncodeFragment)
{
  static const uint8_t expectedBlock[] = {
    0x64, 0x0e, // LpPacket
          0x51, 0x08, // Sequence
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0xe8,
          0x50, 0x02, // Fragment
                0x03, 0xe8,
  };

  Buffer buf(2);
  buf[0] = 0x03;
  buf[1] = 0xe8;

  Packet packet;
  packet.add<FragmentField>(std::make_pair(buf.begin(), buf.end()));
  packet.add<SequenceField>(1000);
  Block wire = packet.wireEncode();
  BOOST_CHECK_EQUAL_COLLECTIONS(expectedBlock, expectedBlock + sizeof(expectedBlock),
                                wire.begin(), wire.end());
}

BOOST_AUTO_TEST_CASE(EncodeSubTlv)
{
  static const uint8_t expectedBlock[] = {
    0x64, 0x09, // LpPacket
          0xfd, 0x03, 0x20, 0x05, // Nack
                0xfd, 0x03, 0x21, 0x01, // NackReason
                      0x64,
  };

  NackHeader nack;
  nack.setReason(NackReason::DUPLICATE);

  Packet packet;
  BOOST_CHECK_NO_THROW(packet.add<NackField>(nack));
  Block wire;
  BOOST_REQUIRE_NO_THROW(wire = packet.wireEncode());
  BOOST_CHECK_EQUAL_COLLECTIONS(expectedBlock, expectedBlock + sizeof(expectedBlock),
                                wire.begin(), wire.end());
}

BOOST_AUTO_TEST_CASE(EncodeZeroLengthTlv)
{
  static const uint8_t expectedBlock[] = {
    0x64, 0x04, // LpPacket
          0xfd, 0x03, 0x4c, 0x00, // NonDiscovery
  };

  Packet packet1, packet2;
  BOOST_CHECK_NO_THROW(packet1.set<NonDiscoveryField>(EmptyValue{}));
  Block wire;
  BOOST_REQUIRE_NO_THROW(wire = packet1.wireEncode());
  BOOST_CHECK_EQUAL_COLLECTIONS(expectedBlock, expectedBlock + sizeof(expectedBlock),
                                wire.begin(), wire.end());

  BOOST_CHECK_NO_THROW(packet2.add<NonDiscoveryField>(EmptyValue{}));
  BOOST_REQUIRE_NO_THROW(wire = packet2.wireEncode());
  BOOST_CHECK_EQUAL_COLLECTIONS(expectedBlock, expectedBlock + sizeof(expectedBlock),
                                wire.begin(), wire.end());
}

BOOST_AUTO_TEST_CASE(EncodeSortOrder)
{
  static const uint8_t expectedBlock[] = {
    0x64, 0x2e, // LpPacket
          0x52, 0x01, // FragIndex
                0x00,
          0x53, 0x01, // FragCount
                0x01,
          0xfd, 0x03, 0x44, 0x08, // Ack
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02,
          0xfd, 0x03, 0x44, 0x08, // Ack
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04,
          0xfd, 0x03, 0x44, 0x08, // Ack
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03,
          0x50, 0x02, // Fragment
                0x03, 0xe8,
  };

  Buffer frag(2);
  frag[0] = 0x03;
  frag[1] = 0xe8;

  Packet packet;
  BOOST_CHECK_NO_THROW(packet.add<FragmentField>(std::make_pair(frag.begin(), frag.end())));
  BOOST_CHECK_NO_THROW(packet.add<FragIndexField>(0));
  BOOST_CHECK_NO_THROW(packet.add<AckField>(2));
  BOOST_REQUIRE_NO_THROW(packet.wireEncode());
  BOOST_CHECK_NO_THROW(packet.add<FragCountField>(1));
  BOOST_REQUIRE_NO_THROW(packet.wireEncode());
  BOOST_CHECK_NO_THROW(packet.add<AckField>(4));
  BOOST_REQUIRE_NO_THROW(packet.wireEncode());
  BOOST_CHECK_NO_THROW(packet.add<AckField>(3));
  BOOST_REQUIRE_NO_THROW(packet.wireEncode());
  Block wire;
  BOOST_REQUIRE_NO_THROW(wire = packet.wireEncode());
  BOOST_CHECK_EQUAL_COLLECTIONS(expectedBlock, expectedBlock + sizeof(expectedBlock),
                                wire.begin(), wire.end());
}

BOOST_AUTO_TEST_CASE(DecodeNormal)
{
  static const uint8_t inputBlock[] = {
    0x64, 0x0a, // LpPacket
          0x52, 0x01, // FragIndex
                0x00,
          0x53, 0x01, // FragCount
                0x01,
          0x50, 0x02, // Fragment
                0x03, 0xe8,
  };

  Packet packet;
  Block wire(inputBlock, sizeof(inputBlock));
  BOOST_CHECK_NO_THROW(packet.wireDecode(wire));
  BOOST_CHECK_EQUAL(1, packet.count<FragmentField>());
  BOOST_CHECK_EQUAL(1, packet.count<FragIndexField>());
  BOOST_CHECK_EQUAL(1, packet.count<FragCountField>());
  Buffer::const_iterator first, last;
  BOOST_REQUIRE_NO_THROW(std::tie(first, last) = packet.get<FragmentField>(0));
  BOOST_CHECK_EQUAL(2, last - first);
  BOOST_CHECK_EQUAL(0x03, *first);
  BOOST_CHECK_EQUAL(0xe8, *(last - 1));
  BOOST_CHECK_EQUAL(0, packet.get<FragIndexField>(0));
  BOOST_CHECK_EQUAL(1, packet.get<FragCountField>(0));
}

BOOST_AUTO_TEST_CASE(DecodeIdle)
{
  static const uint8_t inputBlock[] = {
    0x64, 0x06, // LpPacket
          0x52, 0x01, // FragIndex
                0x00,
          0x53, 0x01, // FragCount
                0x01,
  };

  Packet packet;
  Block wire(inputBlock, sizeof(inputBlock));
  BOOST_CHECK_NO_THROW(packet.wireDecode(wire));
  BOOST_CHECK_EQUAL(0, packet.count<FragmentField>());
  BOOST_CHECK_EQUAL(1, packet.count<FragIndexField>());
  BOOST_CHECK_EQUAL(1, packet.count<FragCountField>());
  BOOST_CHECK_EQUAL(0, packet.get<FragIndexField>(0));
  BOOST_CHECK_EQUAL(1, packet.get<FragCountField>(0));
}

BOOST_AUTO_TEST_CASE(DecodeFragment)
{
  static const uint8_t inputBlock[] = {
    0x64, 0x04, // LpPacket
          0x50, 0x02, // Fragment
                0x03, 0xe8,
  };

  Packet packet;
  Block wire(inputBlock, sizeof(inputBlock));
  BOOST_CHECK_NO_THROW(packet.wireDecode(wire));
  BOOST_CHECK_EQUAL(1, packet.count<FragmentField>());
  BOOST_CHECK_EQUAL(0, packet.count<FragIndexField>());
  Buffer::const_iterator first, last;
  BOOST_REQUIRE_NO_THROW(std::tie(first, last) = packet.get<FragmentField>(0));
  BOOST_CHECK_EQUAL(2, last - first);
  BOOST_CHECK_EQUAL(0x03, *first);
  BOOST_CHECK_EQUAL(0xe8, *(last - 1));
}

BOOST_AUTO_TEST_CASE(DecodeNonDiscoveryHeader)
{
  static const uint8_t inputBlock[] = {
    0x64, 0x04, // LpPacket
          0xfd, 0x03, 0x4c, 0x00, // NonDiscovery
  };

  Packet packet;
  Block wire(inputBlock, sizeof(inputBlock));
  BOOST_CHECK_NO_THROW(packet.wireDecode(wire));
  BOOST_CHECK_EQUAL(true, packet.has<NonDiscoveryField>());
  BOOST_CHECK_NO_THROW(packet.get<NonDiscoveryField>());
}

BOOST_AUTO_TEST_CASE(DecodeEmpty)
{
  static const uint8_t inputBlock[] = {
    0x64, 0x00, // LpPacket
  };

  Packet packet;
  Block wire(inputBlock, sizeof(inputBlock));
  BOOST_CHECK_NO_THROW(packet.wireDecode(wire));
  BOOST_CHECK_EQUAL(0, packet.count<FragmentField>());
  BOOST_CHECK_EQUAL(0, packet.count<FragIndexField>());
  BOOST_CHECK_EQUAL(false, packet.has<NonDiscoveryField>());
}

BOOST_AUTO_TEST_CASE(DecodeRepeatedNonRepeatableHeader)
{
  static const uint8_t inputBlock[] = {
    0x64, 0x06, // LpPacket
          0x52, 0x01, // FragIndex
                0x00,
          0x52, 0x01, // FragIndex
                0x01,
  };

  Packet packet;
  Block wire(inputBlock, sizeof(inputBlock));
  BOOST_CHECK_THROW(packet.wireDecode(wire), Packet::Error);
}

BOOST_AUTO_TEST_CASE(DecodeRepeatedRepeatableHeader)
{
  static const uint8_t inputBlock[] = {
    0x64, 0x0f, // LpPacket
          0xfd, 0x03, 0x44, 0x01, // Ack
                0x01,
          0xfd, 0x03, 0x44, 0x01, // Ack
                0x03,
          0xfd, 0x03, 0x44, 0x01, // Ack
                0x02,
  };

  Packet packet;
  Block wire(inputBlock, sizeof(inputBlock));
  BOOST_CHECK_NO_THROW(packet.wireDecode(wire));
  BOOST_REQUIRE_EQUAL(packet.count<AckField>(), 3);
  BOOST_CHECK_EQUAL(packet.get<AckField>(), 1);
  BOOST_CHECK_EQUAL(packet.get<AckField>(0), 1);
  BOOST_CHECK_EQUAL(packet.get<AckField>(1), 3);
  BOOST_CHECK_EQUAL(packet.get<AckField>(2), 2);
}

BOOST_AUTO_TEST_CASE(DecodeRepeatedFragment)
{
  static const uint8_t inputBlock[] = {
    0x64, 0x08, // LpPacket
          0x50, 0x02, // Fragment
                0x03, 0xe8,
          0x50, 0x02, // Fragment
                0x03, 0xe9,
  };

  Packet packet;
  Block wire(inputBlock, sizeof(inputBlock));
  BOOST_CHECK_THROW(packet.wireDecode(wire), Packet::Error);
}

BOOST_AUTO_TEST_CASE(DecodeWrongOrderAmongHeaders)
{
  static const uint8_t inputBlock[] = {
    0x64, 0x0a, // LpPacket
          0x53, 0x01, // FragCount
                0x01,
          0x52, 0x01, // FragIndex
                0x00,
          0x50, 0x02, // Fragment
                0x03, 0xe8,
  };

  Packet packet;
  Block wire(inputBlock, sizeof(inputBlock));
  BOOST_CHECK_THROW(packet.wireDecode(wire), Packet::Error);
}

BOOST_AUTO_TEST_CASE(DecodeWrongOrderFragment)
{
  static const uint8_t inputBlock[] = {
    0x64, 0x0a, // LpPacket
          0x52, 0x01, // FragIndex
                0x00,
          0x50, 0x02, // Fragment
                0x03, 0xe8,
          0x53, 0x01, // FragCount
                0x01,
  };

  Packet packet;
  Block wire(inputBlock, sizeof(inputBlock));
  BOOST_CHECK_THROW(packet.wireDecode(wire), Packet::Error);
}

BOOST_AUTO_TEST_CASE(DecodeIgnoredHeader)
{
  static const uint8_t inputBlock[] = {
    0x64, 0x0c, // LpPacket
          0x52, 0x01, // FragIndex
                0x00,
          0xfd, 0x03, 0x24, 0x01, // unknown TLV-TYPE 804 (ignored)
                0x02,
          0x50, 0x02, // Fragment
                0x03, 0xe8,
  };

  Packet packet;
  Block wire(inputBlock, sizeof(inputBlock));
  BOOST_CHECK_NO_THROW(packet.wireDecode(wire));
  BOOST_CHECK_EQUAL(1, packet.count<FragmentField>());
  BOOST_CHECK_EQUAL(1, packet.count<FragIndexField>());
}

BOOST_AUTO_TEST_CASE(DecodeUnrecognizedHeader)
{
  static const uint8_t inputBlock[] = {
    0x64, 0x0c, // LpPacket
          0x52, 0x01, // FragIndex
                0x00,
          0xfd, 0x03, 0x22, 0x01, // unknown TLV-TYPE 802 (cannot ignore)
                0x02,
          0x50, 0x02, // Fragment
                0x03, 0xe8,
  };

  Packet packet;
  Block wire(inputBlock, sizeof(inputBlock));
  BOOST_CHECK_THROW(packet.wireDecode(wire), Packet::Error);
}

BOOST_AUTO_TEST_CASE(DecodeBareNetworkLayerPacket)
{
  static const uint8_t inputBlock[] = {
    0x05, 0x0a, // Interest
          0x07, 0x02, // Name
                0x03, 0xe8,
          0x0a, 0x04, // Nonce
                0x01, 0x02, 0x03, 0x04,
  };

  Packet packet;
  Block wire(inputBlock, sizeof(inputBlock));
  BOOST_CHECK_NO_THROW(packet.wireDecode(wire));
  BOOST_CHECK_EQUAL(1, packet.count<FragmentField>());

  Block encoded;
  BOOST_CHECK_NO_THROW(encoded = packet.wireEncode());
  BOOST_CHECK_EQUAL_COLLECTIONS(inputBlock, inputBlock + sizeof(inputBlock),
                                encoded.begin(), encoded.end());
}

BOOST_AUTO_TEST_CASE(DecodeUnrecognizedTlvType)
{
  Packet packet;
  Block wire = encoding::makeEmptyBlock(ndn::tlv::Name);
  BOOST_CHECK_THROW(packet.wireDecode(wire), Packet::Error);
}

BOOST_FIXTURE_TEST_CASE(DecodePrefixAnnouncement, ndn::tests::IdentityManagementFixture)
{
  // Construct Data which prefix announcement is attached to
  Data data0("/edu/ua/cs/news/index.html");
  ndn::SignatureSha256WithRsa fakeSignature;
  fakeSignature.setValue(ndn::encoding::makeEmptyBlock(ndn::tlv::SignatureValue));
  data0.setSignature(fakeSignature);

  Block wire;
  wire = data0.wireEncode();
  Packet packet0;
  packet0.wireDecode(wire);

  // Construct Prefix Announcement
  PrefixAnnouncement pa;
  pa.setAnnouncedName("/net/example");
  pa.setExpiration(5_min);
  pa.setValidityPeriod(security::ValidityPeriod(time::fromIsoString("20181030T000000"),
                                                time::fromIsoString("20181124T235959")));
  pa.toData(m_keyChain, signingWithSha256(), 1);
  PrefixAnnouncementHeader pah0(pa);
  BOOST_CHECK_NO_THROW(packet0.add<PrefixAnnouncementField>(pah0));
  Block encoded;
  BOOST_CHECK_NO_THROW(encoded = packet0.wireEncode());

  // check decoding
  Packet packet1;
  BOOST_CHECK_NO_THROW(packet1.wireDecode(encoded));
  BOOST_CHECK_EQUAL(true, packet1.has<PrefixAnnouncementField>());
  PrefixAnnouncementHeader pah1;
  BOOST_CHECK_NO_THROW(pah1 = packet1.get<PrefixAnnouncementField>());
  BOOST_CHECK_EQUAL(pah1.getPrefixAnn()->getAnnouncedName(), "/net/example");
}

BOOST_AUTO_TEST_SUITE_END() // TestPacket
BOOST_AUTO_TEST_SUITE_END() // Lp

} // namespace tests
} // namespace lp
} // namespace ndn
