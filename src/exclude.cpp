/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2013-2014 Regents of the University of California.
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
 *
 * @author Alexander Afanasyev <http://lasr.cs.ucla.edu/afanasyev/index.html>
 */

#include "exclude.hpp"
#include "util/concepts.hpp"

namespace ndn {

BOOST_CONCEPT_ASSERT((boost::EqualityComparable<Exclude>));
BOOST_CONCEPT_ASSERT((WireEncodable<Exclude>));
BOOST_CONCEPT_ASSERT((WireDecodable<Exclude>));
static_assert(std::is_base_of<tlv::Error, Exclude::Error>::value,
              "Exclude::Error must inherit from tlv::Error");

Exclude::Exclude()
{
}

Exclude::Exclude(const Block& wire)
{
  wireDecode(wire);
}

template<bool T>
size_t
Exclude::wireEncode(EncodingImpl<T>& block) const
{
  if (m_exclude.empty()) {
    throw Error("Exclude filter cannot be empty");
  }

  size_t totalLength = 0;

  // Exclude ::= EXCLUDE-TYPE TLV-LENGTH Any? (NameComponent (Any)?)+
  // Any     ::= ANY-TYPE TLV-LENGTH(=0)

  for (Exclude::const_iterator i = m_exclude.begin(); i != m_exclude.end(); i++)
    {
      if (i->second)
        {
          totalLength += prependBooleanBlock(block, tlv::Any);
        }
      if (!i->first.empty())
        {
          totalLength += i->first.wireEncode(block);
        }
    }

  totalLength += block.prependVarNumber(totalLength);
  totalLength += block.prependVarNumber(tlv::Exclude);
  return totalLength;
}

template size_t
Exclude::wireEncode<true>(EncodingImpl<true>& block) const;

template size_t
Exclude::wireEncode<false>(EncodingImpl<false>& block) const;

const Block&
Exclude::wireEncode() const
{
  if (m_wire.hasWire())
    return m_wire;

  EncodingEstimator estimator;
  size_t estimatedSize = wireEncode(estimator);

  EncodingBuffer buffer(estimatedSize, 0);
  wireEncode(buffer);

  m_wire = buffer.block();
  return m_wire;
}

void
Exclude::wireDecode(const Block& wire)
{
  clear();

  if (wire.type() != tlv::Exclude)
    throw tlv::Error("Unexpected TLV type when decoding Exclude");

  m_wire = wire;
  m_wire.parse();

  if (m_wire.elements_size() == 0) {
    throw Error("Exclude element cannot be empty");
  }

  // Exclude ::= EXCLUDE-TYPE TLV-LENGTH Any? (NameComponent (Any)?)+
  // Any     ::= ANY-TYPE TLV-LENGTH(=0)

  Block::element_const_iterator i = m_wire.elements_begin();
  if (i->type() == tlv::Any)
    {
      appendExclude(name::Component(), true);
      ++i;
    }

  while (i != m_wire.elements_end())
    {
      if (i->type() != tlv::NameComponent)
        throw Error("Incorrect format of Exclude filter");

      name::Component excludedComponent(i->value(), i->value_size());
      ++i;

      if (i != m_wire.elements_end())
        {
          if (i->type() == tlv::Any)
            {
              appendExclude(excludedComponent, true);
              ++i;
            }
          else
            {
              appendExclude(excludedComponent, false);
            }
        }
      else
        {
          appendExclude(excludedComponent, false);
        }
    }
}

// example: ANY /b /d ANY /f
//
// ordered in map as:
//
// /f (false); /d (true); /b (false); / (true)
//
// lower_bound(/)  -> / (true) <-- excluded (equal)
// lower_bound(/a) -> / (true) <-- excluded (any)
// lower_bound(/b) -> /b (false) <--- excluded (equal)
// lower_bound(/c) -> /b (false) <--- not excluded (not equal and no ANY)
// lower_bound(/d) -> /d (true) <- excluded
// lower_bound(/e) -> /d (true) <- excluded
bool
Exclude::isExcluded(const name::Component& comp) const
{
  const_iterator lowerBound = m_exclude.lower_bound(comp);
  if (lowerBound == end())
    return false;

  if (lowerBound->second)
    return true;
  else
    return lowerBound->first == comp;

  return false;
}

Exclude&
Exclude::excludeOne(const name::Component& comp)
{
  if (!isExcluded(comp))
    {
      m_exclude.insert(std::make_pair(comp, false));
      m_wire.reset();
    }
  return *this;
}

// example: ANY /b0 /d0 ANY /f0
//
// ordered in map as:
//
// /f0 (false); /d0 (true); /b0 (false); / (true)
//
// lower_bound(/)  -> / (true) <-- excluded (equal)
// lower_bound(/a0) -> / (true) <-- excluded (any)
// lower_bound(/b0) -> /b0 (false) <--- excluded (equal)
// lower_bound(/c0) -> /b0 (false) <--- not excluded (not equal and no ANY)
// lower_bound(/d0) -> /d0 (true) <- excluded
// lower_bound(/e0) -> /d0 (true) <- excluded


// examples with desired outcomes
// excludeRange(/, /f0) ->  ANY /f0
//                          /f0 (false); / (true)
// excludeRange(/, /f1) ->  ANY /f1
//                          /f1 (false); / (true)
// excludeRange(/a0, /e0) ->  ANY /f0
//                          /f0 (false); / (true)
// excludeRange(/a0, /e0) ->  ANY /f0
//                          /f0 (false); / (true)

// excludeRange(/b1, /c0) ->  ANY /b0 /b1 ANY /c0 /d0 ANY /f0
//                          /f0 (false); /d0 (true); /c0 (false); /b1 (true); /b0 (false); / (true)

Exclude&
Exclude::excludeRange(const name::Component& from, const name::Component& to)
{
  if (from >= to) {
    throw Error("Invalid exclude range [" + from.toUri() + ", " + to.toUri() + "] "
                "(for single name exclude use Exclude::excludeOne)");
  }

  iterator newFrom = m_exclude.lower_bound(from);
  if (newFrom == end() || !newFrom->second /*without ANY*/) {
    std::pair<iterator, bool> fromResult = m_exclude.insert(std::make_pair(from, true));
    newFrom = fromResult.first;
    if (!fromResult.second) {
      // this means that the lower bound is equal to the item itself. So, just update ANY flag
      newFrom->second = true;
    }
  }
  // else
  // nothing special if start of the range already exists with ANY flag set

  iterator newTo = m_exclude.lower_bound(to); // !newTo cannot be end()
  if (newTo == newFrom || !newTo->second) {
      std::pair<iterator, bool> toResult = m_exclude.insert(std::make_pair(to, false));
      newTo = toResult.first;
      ++ newTo;
  }
  // else
  // nothing to do really

  m_exclude.erase(newTo, newFrom); // remove any intermediate node, since all of the are excluded

  m_wire.reset();
  return *this;
}

Exclude&
Exclude::excludeAfter(const name::Component& from)
{
  iterator newFrom = m_exclude.lower_bound(from);
  if (newFrom == end() || !newFrom->second /*without ANY*/) {
    std::pair<iterator, bool> fromResult = m_exclude.insert(std::make_pair(from, true));
    newFrom = fromResult.first;
    if (!fromResult.second) {
      // this means that the lower bound is equal to the item itself. So, just update ANY flag
      newFrom->second = true;
    }
  }
  // else
  // nothing special if start of the range already exists with ANY flag set

  if (newFrom != m_exclude.begin()) {
    // remove any intermediate node, since all of the are excluded
    m_exclude.erase(m_exclude.begin(), newFrom);
  }

  m_wire.reset();
  return *this;
}

std::ostream&
operator<<(std::ostream& os, const Exclude& exclude)
{
  bool empty = true;
  for (Exclude::const_reverse_iterator i = exclude.rbegin(); i != exclude.rend(); i++) {
    if (!i->first.empty()) {
      if (!empty) os << ",";
      os << i->first.toUri();
      empty = false;
    }
    if (i->second) {
      if (!empty) os << ",";
      os << "*";
      empty = false;
    }
  }
  return os;
}

std::string
Exclude::toUri() const
{
  std::ostringstream os;
  os << *this;
  return os.str();
}

bool
Exclude::operator==(const Exclude& other) const
{
  if (empty() && other.empty())
    return true;
  if (empty() || other.empty())
    return false;

  return wireEncode() == other.wireEncode();
}

} // namespace ndn
