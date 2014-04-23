/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil -*- */
/**
 * @author: Jeff Thompson <jefft0@remap.ucla.edu>
 * Derived from command-interest-generator.hpp by the NFD authors: https://github.com/named-data/NFD/blob/master/AUTHORS.md .
 * See COPYING for copyright and distribution information.
 */

#ifndef NDN_NFD_COMMAND_INTEREST_GENERATOR_HPP
#define NDN_NFD_COMMAND_INTEREST_GENERATOR_HPP

#include <ndn-cpp/interest.hpp>

namespace ndn {

class KeyChain;
  
/** An NfdCommandInterestGenerator keeps track of a timestamp and generates
 * command interests according to the NFD Signed Command Interests protocol:
 * http://redmine.named-data.net/projects/nfd/wiki/Command_Interests
 */
class NfdCommandInterestGenerator {
public:
  /**
   * Create a new NfdCommandInterestGenerator and initialize the timestamp to 
   * now.
   */
  NfdCommandInterestGenerator();

  /**
   * Append a timestamp component and a random value component to interest's
   * name. This ensures that the timestamp is greater than the timestamp used in
   * the previous call. Then use keyChain to sign the interest which appends a
   * SignatureInfo component and a component with the signature bits. If the
   * interest lifetime is not set, this sets it.
   * @param interest The interest whose name is append with components.
   * @param keyChain The KeyChain for calling sign.
   * @param certificateName The certificate name of the key to use for signing.
   * @param wireFormat A WireFormat object used to encode the SignatureInfo and
   * to encode interest name for signing. If omitted, use 
   * WireFormat getDefaultWireFormat().
   */
  void
  generate
    (Interest& interest, KeyChain& keyChain, const Name& certificateName, 
     WireFormat& wireFormat = *WireFormat::getDefaultWireFormat());

private:
  MillisecondsSince1970 lastTimestamp_;
};

}

#endif