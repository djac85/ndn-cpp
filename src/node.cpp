/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil -*- */
/**
 * Copyright (C) 2013-2015 Regents of the University of California.
 * @author: Jeff Thompson <jefft0@remap.ucla.edu>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version, with the additional exemption that
 * compiling, linking, and/or using OpenSSL is allowed.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * A copy of the GNU Lesser General Public License is in the file COPYING.
 */

#include <stdexcept>
// TODO: Remove this include when we remove selfregSignand NdndIdFetcher.
#include <openssl/ssl.h>
#include <ndn-cpp/encoding/tlv-wire-format.hpp>
#include <ndn-cpp/forwarding-entry.hpp>
#include <ndn-cpp/security/key-chain.hpp>
#include <ndn-cpp/sha256-with-rsa-signature.hpp>
#include <ndn-cpp/control-parameters.hpp>
#include <ndn-cpp/encoding/binary-xml-wire-format.hpp>
#include "c/name.h"
#include "c/interest.h"
#include "c/util/crypto.h"
#include "c/util/time.h"
#include "encoding/tlv-decoder.hpp"
#include "c/encoding/binary-xml.h"
#include "encoding/binary-xml-decoder.hpp"
#include "util/logging.hpp"
#include "node.hpp"

INIT_LOGGER("ndn.Node");

using namespace std;
using namespace ndn::func_lib;

namespace ndn {

static uint8_t SELFREG_PUBLIC_KEY_DER[] = {
  0x30, 0x82, 0x01, 0x22, 0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01,
  0x01, 0x05, 0x00, 0x03, 0x82, 0x01, 0x0f, 0x00, 0x30, 0x82, 0x01, 0x0a, 0x02, 0x82, 0x01, 0x01,
  0x00, 0xb8, 0x09, 0xa7, 0x59, 0x82, 0x84, 0xec, 0x4f, 0x06, 0xfa, 0x1c, 0xb2, 0xe1, 0x38, 0x93,
  0x53, 0xbb, 0x7d, 0xd4, 0xac, 0x88, 0x1a, 0xf8, 0x25, 0x11, 0xe4, 0xfa, 0x1d, 0x61, 0x24, 0x5b,
  0x82, 0xca, 0xcd, 0x72, 0xce, 0xdb, 0x66, 0xb5, 0x8d, 0x54, 0xbd, 0xfb, 0x23, 0xfd, 0xe8, 0x8e,
  0xaf, 0xa7, 0xb3, 0x79, 0xbe, 0x94, 0xb5, 0xb7, 0xba, 0x17, 0xb6, 0x05, 0xae, 0xce, 0x43, 0xbe,
  0x3b, 0xce, 0x6e, 0xea, 0x07, 0xdb, 0xbf, 0x0a, 0x7e, 0xeb, 0xbc, 0xc9, 0x7b, 0x62, 0x3c, 0xf5,
  0xe1, 0xce, 0xe1, 0xd9, 0x8d, 0x9c, 0xfe, 0x1f, 0xc7, 0xf8, 0xfb, 0x59, 0xc0, 0x94, 0x0b, 0x2c,
  0xd9, 0x7d, 0xbc, 0x96, 0xeb, 0xb8, 0x79, 0x22, 0x8a, 0x2e, 0xa0, 0x12, 0x1d, 0x42, 0x07, 0xb6,
  0x5d, 0xdb, 0xe1, 0xf6, 0xb1, 0x5d, 0x7b, 0x1f, 0x54, 0x52, 0x1c, 0xa3, 0x11, 0x9b, 0xf9, 0xeb,
  0xbe, 0xb3, 0x95, 0xca, 0xa5, 0x87, 0x3f, 0x31, 0x18, 0x1a, 0xc9, 0x99, 0x01, 0xec, 0xaa, 0x90,
  0xfd, 0x8a, 0x36, 0x35, 0x5e, 0x12, 0x81, 0xbe, 0x84, 0x88, 0xa1, 0x0d, 0x19, 0x2a, 0x4a, 0x66,
  0xc1, 0x59, 0x3c, 0x41, 0x83, 0x3d, 0x3d, 0xb8, 0xd4, 0xab, 0x34, 0x90, 0x06, 0x3e, 0x1a, 0x61,
  0x74, 0xbe, 0x04, 0xf5, 0x7a, 0x69, 0x1b, 0x9d, 0x56, 0xfc, 0x83, 0xb7, 0x60, 0xc1, 0x5e, 0x9d,
  0x85, 0x34, 0xfd, 0x02, 0x1a, 0xba, 0x2c, 0x09, 0x72, 0xa7, 0x4a, 0x5e, 0x18, 0xbf, 0xc0, 0x58,
  0xa7, 0x49, 0x34, 0x46, 0x61, 0x59, 0x0e, 0xe2, 0x6e, 0x9e, 0xd2, 0xdb, 0xfd, 0x72, 0x2f, 0x3c,
  0x47, 0xcc, 0x5f, 0x99, 0x62, 0xee, 0x0d, 0xf3, 0x1f, 0x30, 0x25, 0x20, 0x92, 0x15, 0x4b, 0x04,
  0xfe, 0x15, 0x19, 0x1d, 0xdc, 0x7e, 0x5c, 0x10, 0x21, 0x52, 0x21, 0x91, 0x54, 0x60, 0x8b, 0x92,
  0x41, 0x02, 0x03, 0x01, 0x00, 0x01
};

static uint8_t SELFREG_PRIVATE_KEY_DER[] = {
  0x30, 0x82, 0x04, 0xa5, 0x02, 0x01, 0x00, 0x02, 0x82, 0x01, 0x01, 0x00, 0xb8, 0x09, 0xa7, 0x59,
  0x82, 0x84, 0xec, 0x4f, 0x06, 0xfa, 0x1c, 0xb2, 0xe1, 0x38, 0x93, 0x53, 0xbb, 0x7d, 0xd4, 0xac,
  0x88, 0x1a, 0xf8, 0x25, 0x11, 0xe4, 0xfa, 0x1d, 0x61, 0x24, 0x5b, 0x82, 0xca, 0xcd, 0x72, 0xce,
  0xdb, 0x66, 0xb5, 0x8d, 0x54, 0xbd, 0xfb, 0x23, 0xfd, 0xe8, 0x8e, 0xaf, 0xa7, 0xb3, 0x79, 0xbe,
  0x94, 0xb5, 0xb7, 0xba, 0x17, 0xb6, 0x05, 0xae, 0xce, 0x43, 0xbe, 0x3b, 0xce, 0x6e, 0xea, 0x07,
  0xdb, 0xbf, 0x0a, 0x7e, 0xeb, 0xbc, 0xc9, 0x7b, 0x62, 0x3c, 0xf5, 0xe1, 0xce, 0xe1, 0xd9, 0x8d,
  0x9c, 0xfe, 0x1f, 0xc7, 0xf8, 0xfb, 0x59, 0xc0, 0x94, 0x0b, 0x2c, 0xd9, 0x7d, 0xbc, 0x96, 0xeb,
  0xb8, 0x79, 0x22, 0x8a, 0x2e, 0xa0, 0x12, 0x1d, 0x42, 0x07, 0xb6, 0x5d, 0xdb, 0xe1, 0xf6, 0xb1,
  0x5d, 0x7b, 0x1f, 0x54, 0x52, 0x1c, 0xa3, 0x11, 0x9b, 0xf9, 0xeb, 0xbe, 0xb3, 0x95, 0xca, 0xa5,
  0x87, 0x3f, 0x31, 0x18, 0x1a, 0xc9, 0x99, 0x01, 0xec, 0xaa, 0x90, 0xfd, 0x8a, 0x36, 0x35, 0x5e,
  0x12, 0x81, 0xbe, 0x84, 0x88, 0xa1, 0x0d, 0x19, 0x2a, 0x4a, 0x66, 0xc1, 0x59, 0x3c, 0x41, 0x83,
  0x3d, 0x3d, 0xb8, 0xd4, 0xab, 0x34, 0x90, 0x06, 0x3e, 0x1a, 0x61, 0x74, 0xbe, 0x04, 0xf5, 0x7a,
  0x69, 0x1b, 0x9d, 0x56, 0xfc, 0x83, 0xb7, 0x60, 0xc1, 0x5e, 0x9d, 0x85, 0x34, 0xfd, 0x02, 0x1a,
  0xba, 0x2c, 0x09, 0x72, 0xa7, 0x4a, 0x5e, 0x18, 0xbf, 0xc0, 0x58, 0xa7, 0x49, 0x34, 0x46, 0x61,
  0x59, 0x0e, 0xe2, 0x6e, 0x9e, 0xd2, 0xdb, 0xfd, 0x72, 0x2f, 0x3c, 0x47, 0xcc, 0x5f, 0x99, 0x62,
  0xee, 0x0d, 0xf3, 0x1f, 0x30, 0x25, 0x20, 0x92, 0x15, 0x4b, 0x04, 0xfe, 0x15, 0x19, 0x1d, 0xdc,
  0x7e, 0x5c, 0x10, 0x21, 0x52, 0x21, 0x91, 0x54, 0x60, 0x8b, 0x92, 0x41, 0x02, 0x03, 0x01, 0x00,
  0x01, 0x02, 0x82, 0x01, 0x01, 0x00, 0x8a, 0x05, 0xfb, 0x73, 0x7f, 0x16, 0xaf, 0x9f, 0xa9, 0x4c,
  0xe5, 0x3f, 0x26, 0xf8, 0x66, 0x4d, 0xd2, 0xfc, 0xd1, 0x06, 0xc0, 0x60, 0xf1, 0x9f, 0xe3, 0xa6,
  0xc6, 0x0a, 0x48, 0xb3, 0x9a, 0xca, 0x21, 0xcd, 0x29, 0x80, 0x88, 0x3d, 0xa4, 0x85, 0xa5, 0x7b,
  0x82, 0x21, 0x81, 0x28, 0xeb, 0xf2, 0x43, 0x24, 0xb0, 0x76, 0xc5, 0x52, 0xef, 0xc2, 0xea, 0x4b,
  0x82, 0x41, 0x92, 0xc2, 0x6d, 0xa6, 0xae, 0xf0, 0xb2, 0x26, 0x48, 0xa1, 0x23, 0x7f, 0x02, 0xcf,
  0xa8, 0x90, 0x17, 0xa2, 0x3e, 0x8a, 0x26, 0xbd, 0x6d, 0x8a, 0xee, 0xa6, 0x0c, 0x31, 0xce, 0xc2,
  0xbb, 0x92, 0x59, 0xb5, 0x73, 0xe2, 0x7d, 0x91, 0x75, 0xe2, 0xbd, 0x8c, 0x63, 0xe2, 0x1c, 0x8b,
  0xc2, 0x6a, 0x1c, 0xfe, 0x69, 0xc0, 0x44, 0xcb, 0x58, 0x57, 0xb7, 0x13, 0x42, 0xf0, 0xdb, 0x50,
  0x4c, 0xe0, 0x45, 0x09, 0x8f, 0xca, 0x45, 0x8a, 0x06, 0xfe, 0x98, 0xd1, 0x22, 0xf5, 0x5a, 0x9a,
  0xdf, 0x89, 0x17, 0xca, 0x20, 0xcc, 0x12, 0xa9, 0x09, 0x3d, 0xd5, 0xf7, 0xe3, 0xeb, 0x08, 0x4a,
  0xc4, 0x12, 0xc0, 0xb9, 0x47, 0x6c, 0x79, 0x50, 0x66, 0xa3, 0xf8, 0xaf, 0x2c, 0xfa, 0xb4, 0x6b,
  0xec, 0x03, 0xad, 0xcb, 0xda, 0x24, 0x0c, 0x52, 0x07, 0x87, 0x88, 0xc0, 0x21, 0xf3, 0x02, 0xe8,
  0x24, 0x44, 0x0f, 0xcd, 0xa0, 0xad, 0x2f, 0x1b, 0x79, 0xab, 0x6b, 0x49, 0x4a, 0xe6, 0x3b, 0xd0,
  0xad, 0xc3, 0x48, 0xb9, 0xf7, 0xf1, 0x34, 0x09, 0xeb, 0x7a, 0xc0, 0xd5, 0x0d, 0x39, 0xd8, 0x45,
  0xce, 0x36, 0x7a, 0xd8, 0xde, 0x3c, 0xb0, 0x21, 0x96, 0x97, 0x8a, 0xff, 0x8b, 0x23, 0x60, 0x4f,
  0xf0, 0x3d, 0xd7, 0x8f, 0xf3, 0x2c, 0xcb, 0x1d, 0x48, 0x3f, 0x86, 0xc4, 0xa9, 0x00, 0xf2, 0x23,
  0x2d, 0x72, 0x4d, 0x66, 0xa5, 0x01, 0x02, 0x81, 0x81, 0x00, 0xdc, 0x4f, 0x99, 0x44, 0x0d, 0x7f,
  0x59, 0x46, 0x1e, 0x8f, 0xe7, 0x2d, 0x8d, 0xdd, 0x54, 0xc0, 0xf7, 0xfa, 0x46, 0x0d, 0x9d, 0x35,
  0x03, 0xf1, 0x7c, 0x12, 0xf3, 0x5a, 0x9d, 0x83, 0xcf, 0xdd, 0x37, 0x21, 0x7c, 0xb7, 0xee, 0xc3,
  0x39, 0xd2, 0x75, 0x8f, 0xb2, 0x2d, 0x6f, 0xec, 0xc6, 0x03, 0x55, 0xd7, 0x00, 0x67, 0xd3, 0x9b,
  0xa2, 0x68, 0x50, 0x6f, 0x9e, 0x28, 0xa4, 0x76, 0x39, 0x2b, 0xb2, 0x65, 0xcc, 0x72, 0x82, 0x93,
  0xa0, 0xcf, 0x10, 0x05, 0x6a, 0x75, 0xca, 0x85, 0x35, 0x99, 0xb0, 0xa6, 0xc6, 0xef, 0x4c, 0x4d,
  0x99, 0x7d, 0x2c, 0x38, 0x01, 0x21, 0xb5, 0x31, 0xac, 0x80, 0x54, 0xc4, 0x18, 0x4b, 0xfd, 0xef,
  0xb3, 0x30, 0x22, 0x51, 0x5a, 0xea, 0x7d, 0x9b, 0xb2, 0x9d, 0xcb, 0xba, 0x3f, 0xc0, 0x1a, 0x6b,
  0xcd, 0xb0, 0xe6, 0x2f, 0x04, 0x33, 0xd7, 0x3a, 0x49, 0x71, 0x02, 0x81, 0x81, 0x00, 0xd5, 0xd9,
  0xc9, 0x70, 0x1a, 0x13, 0xb3, 0x39, 0x24, 0x02, 0xee, 0xb0, 0xbb, 0x84, 0x17, 0x12, 0xc6, 0xbd,
  0x65, 0x73, 0xe9, 0x34, 0x5d, 0x43, 0xff, 0xdc, 0xf8, 0x55, 0xaf, 0x2a, 0xb9, 0xe1, 0xfa, 0x71,
  0x65, 0x4e, 0x50, 0x0f, 0xa4, 0x3b, 0xe5, 0x68, 0xf2, 0x49, 0x71, 0xaf, 0x15, 0x88, 0xd7, 0xaf,
  0xc4, 0x9d, 0x94, 0x84, 0x6b, 0x5b, 0x10, 0xd5, 0xc0, 0xaa, 0x0c, 0x13, 0x62, 0x99, 0xc0, 0x8b,
  0xfc, 0x90, 0x0f, 0x87, 0x40, 0x4d, 0x58, 0x88, 0xbd, 0xe2, 0xba, 0x3e, 0x7e, 0x2d, 0xd7, 0x69,
  0xa9, 0x3c, 0x09, 0x64, 0x31, 0xb6, 0xcc, 0x4d, 0x1f, 0x23, 0xb6, 0x9e, 0x65, 0xd6, 0x81, 0xdc,
  0x85, 0xcc, 0x1e, 0xf1, 0x0b, 0x84, 0x38, 0xab, 0x93, 0x5f, 0x9f, 0x92, 0x4e, 0x93, 0x46, 0x95,
  0x6b, 0x3e, 0xb6, 0xc3, 0x1b, 0xd7, 0x69, 0xa1, 0x0a, 0x97, 0x37, 0x78, 0xed, 0xd1, 0x02, 0x81,
  0x80, 0x33, 0x18, 0xc3, 0x13, 0x65, 0x8e, 0x03, 0xc6, 0x9f, 0x90, 0x00, 0xae, 0x30, 0x19, 0x05,
  0x6f, 0x3c, 0x14, 0x6f, 0xea, 0xf8, 0x6b, 0x33, 0x5e, 0xee, 0xc7, 0xf6, 0x69, 0x2d, 0xdf, 0x44,
  0x76, 0xaa, 0x32, 0xba, 0x1a, 0x6e, 0xe6, 0x18, 0xa3, 0x17, 0x61, 0x1c, 0x92, 0x2d, 0x43, 0x5d,
  0x29, 0xa8, 0xdf, 0x14, 0xd8, 0xff, 0xdb, 0x38, 0xef, 0xb8, 0xb8, 0x2a, 0x96, 0x82, 0x8e, 0x68,
  0xf4, 0x19, 0x8c, 0x42, 0xbe, 0xcc, 0x4a, 0x31, 0x21, 0xd5, 0x35, 0x6c, 0x5b, 0xa5, 0x7c, 0xff,
  0xd1, 0x85, 0x87, 0x28, 0xdc, 0x97, 0x75, 0xe8, 0x03, 0x80, 0x1d, 0xfd, 0x25, 0x34, 0x41, 0x31,
  0x21, 0x12, 0x87, 0xe8, 0x9a, 0xb7, 0x6a, 0xc0, 0xc4, 0x89, 0x31, 0x15, 0x45, 0x0d, 0x9c, 0xee,
  0xf0, 0x6a, 0x2f, 0xe8, 0x59, 0x45, 0xc7, 0x7b, 0x0d, 0x6c, 0x55, 0xbb, 0x43, 0xca, 0xc7, 0x5a,
  0x01, 0x02, 0x81, 0x81, 0x00, 0xab, 0xf4, 0xd5, 0xcf, 0x78, 0x88, 0x82, 0xc2, 0xdd, 0xbc, 0x25,
  0xe6, 0xa2, 0xc1, 0xd2, 0x33, 0xdc, 0xef, 0x0a, 0x97, 0x2b, 0xdc, 0x59, 0x6a, 0x86, 0x61, 0x4e,
  0xa6, 0xc7, 0x95, 0x99, 0xa6, 0xa6, 0x55, 0x6c, 0x5a, 0x8e, 0x72, 0x25, 0x63, 0xac, 0x52, 0xb9,
  0x10, 0x69, 0x83, 0x99, 0xd3, 0x51, 0x6c, 0x1a, 0xb3, 0x83, 0x6a, 0xff, 0x50, 0x58, 0xb7, 0x28,
  0x97, 0x13, 0xe2, 0xba, 0x94, 0x5b, 0x89, 0xb4, 0xea, 0xba, 0x31, 0xcd, 0x78, 0xe4, 0x4a, 0x00,
  0x36, 0x42, 0x00, 0x62, 0x41, 0xc6, 0x47, 0x46, 0x37, 0xea, 0x6d, 0x50, 0xb4, 0x66, 0x8f, 0x55,
  0x0c, 0xc8, 0x99, 0x91, 0xd5, 0xec, 0xd2, 0x40, 0x1c, 0x24, 0x7d, 0x3a, 0xff, 0x74, 0xfa, 0x32,
  0x24, 0xe0, 0x11, 0x2b, 0x71, 0xad, 0x7e, 0x14, 0xa0, 0x77, 0x21, 0x68, 0x4f, 0xcc, 0xb6, 0x1b,
  0xe8, 0x00, 0x49, 0x13, 0x21, 0x02, 0x81, 0x81, 0x00, 0xb6, 0x18, 0x73, 0x59, 0x2c, 0x4f, 0x92,
  0xac, 0xa2, 0x2e, 0x5f, 0xb6, 0xbe, 0x78, 0x5d, 0x47, 0x71, 0x04, 0x92, 0xf0, 0xd7, 0xe8, 0xc5,
  0x7a, 0x84, 0x6b, 0xb8, 0xb4, 0x30, 0x1f, 0xd8, 0x0d, 0x58, 0xd0, 0x64, 0x80, 0xa7, 0x21, 0x1a,
  0x48, 0x00, 0x37, 0xd6, 0x19, 0x71, 0xbb, 0x91, 0x20, 0x9d, 0xe2, 0xc3, 0xec, 0xdb, 0x36, 0x1c,
  0xca, 0x48, 0x7d, 0x03, 0x32, 0x74, 0x1e, 0x65, 0x73, 0x02, 0x90, 0x73, 0xd8, 0x3f, 0xb5, 0x52,
  0x35, 0x79, 0x1c, 0xee, 0x93, 0xa3, 0x32, 0x8b, 0xed, 0x89, 0x98, 0xf1, 0x0c, 0xd8, 0x12, 0xf2,
  0x89, 0x7f, 0x32, 0x23, 0xec, 0x67, 0x66, 0x52, 0x83, 0x89, 0x99, 0x5e, 0x42, 0x2b, 0x42, 0x4b,
  0x84, 0x50, 0x1b, 0x3e, 0x47, 0x6d, 0x74, 0xfb, 0xd1, 0xa6, 0x10, 0x20, 0x6c, 0x6e, 0xbe, 0x44,
  0x3f, 0xb9, 0xfe, 0xbc, 0x8d, 0xda, 0xcb, 0xea, 0x8f
};

/**
 * Set the KeyLocator using the full SELFREG_PUBLIC_KEY_DER, sign the data packet using SELFREG_PRIVATE_KEY_DER
 * and set the signature.
 * This is a temporary function, because we expect in the future that registerPrefix will not require a signature on the packet.
 * @param data The Data packet to sign.
 * @param wireFormat The WireFormat for encoding the Data packet.
 */
static void
selfregSign(Data& data, WireFormat& wireFormat)
{
  data.setSignature(Sha256WithRsaSignature());
  Sha256WithRsaSignature *signature = dynamic_cast<Sha256WithRsaSignature*>(data.getSignature());

  // Set the public key.
  uint8_t publicKeyDigest[SHA256_DIGEST_LENGTH];
  ndn_digestSha256(SELFREG_PUBLIC_KEY_DER, sizeof(SELFREG_PUBLIC_KEY_DER), publicKeyDigest);
  // Since we encode the register prefix message as BinaryXml, use the full
  //   public key in the key locator to make the legacy NDNx happy.
  signature->getPublisherPublicKeyDigest().setPublisherPublicKeyDigest(Blob(publicKeyDigest, sizeof(publicKeyDigest)));
  signature->getKeyLocator().setType(ndn_KeyLocatorType_KEY);
  signature->getKeyLocator().setKeyData(Blob(SELFREG_PUBLIC_KEY_DER, sizeof(SELFREG_PUBLIC_KEY_DER)));

  // Sign the fields.
  SignedBlob encoding = data.wireEncode(wireFormat);
  uint8_t signedPortionDigest[SHA256_DIGEST_LENGTH];
  ndn_digestSha256(encoding.signedBuf(), encoding.signedSize(), signedPortionDigest);
  uint8_t signatureBits[1000];
  unsigned int signatureBitsLength;
  // Use a temporary pointer since d2i updates it.
  const uint8_t *derPointer = SELFREG_PRIVATE_KEY_DER;
  RSA *privateKey = d2i_RSAPrivateKey(NULL, &derPointer, sizeof(SELFREG_PRIVATE_KEY_DER));
  if (!privateKey)
    throw runtime_error("Error decoding private key in d2i_RSAPrivateKey");
  int success = RSA_sign(NID_sha256, signedPortionDigest, sizeof(signedPortionDigest), signatureBits, &signatureBitsLength, privateKey);
  // Free the private key before checking for success.
  RSA_free(privateKey);
  if (!success)
    throw runtime_error("Error in RSA_sign");

  signature->setSignature(Blob(signatureBits, (size_t)signatureBitsLength));
}

Node::Node(const ptr_lib::shared_ptr<Transport>& transport, const ptr_lib::shared_ptr<const Transport::ConnectionInfo>& connectionInfo)
: transport_(transport), connectionInfo_(connectionInfo),
  ndndIdFetcherInterest_(Name("/%C1.M.S.localhost/%C1.M.SRV/ndnd/KEY"), 4000.0),
  timeoutPrefix_(Name("/local/timeout")),
  lastEntryId_(0)
{
}

static void dummyOnConnected()
{
  // TODO: Implement onConnected. For now, do nothing.
}

void
Node::expressInterest
  (uint64_t pendingInterestId, const Interest& interest, const OnData& onData,
   const OnTimeout& onTimeout, WireFormat& wireFormat, Face* face)
{
  ptr_lib::shared_ptr<const Interest> interestCopy(new Interest(interest));
  
  // TODO: Properly check if we are already connected to the expected host.
  if (!transport_->getIsConnected())
    transport_->connect(*connectionInfo_, *this, &dummyOnConnected);

  expressInterestHelper
    (pendingInterestId, interestCopy, onData, onTimeout, wireFormat, face);
}

void
Node::removePendingInterest(uint64_t pendingInterestId)
{
  int count = 0;
  // Go backwards through the list so we can erase entries.
  // Remove all entries even though pendingInterestId should be unique.
  for (int i = (int)pendingInterestTable_.size() - 1; i >= 0; --i) {
    if (pendingInterestTable_[i]->getPendingInterestId() == pendingInterestId) {
      ++count;
      // For efficiency, mark this as removed so that processInterestTimeout
      // doesn't look for it.
      pendingInterestTable_[i]->setIsRemoved();
      pendingInterestTable_.erase(pendingInterestTable_.begin() + i);
    }
  }

  if (count == 0)
    _LOG_DEBUG("removePendingInterest: Didn't find pendingInterestId " << pendingInterestId);
}

void
Node::registerPrefix
  (uint64_t registeredPrefixId, const Name& prefix,
   const OnInterestCallback& onInterest,
   const OnRegisterFailed& onRegisterFailed, const ForwardingFlags& flags,
   WireFormat& wireFormat, KeyChain& commandKeyChain,
   const Name& commandCertificateName, Face* face)
{
  // If we have an _ndndId, we know we already connected to NDNx.
  if (ndndId_.size() != 0 || !&commandKeyChain) {
    // Assume we are connected to a legacy NDNx server.
    if (!WireFormat::ENABLE_NDNX)
      throw runtime_error
        ("registerPrefix with NDNx is deprecated. To enable while you upgrade your code to use NFD, set WireFormat::ENABLE_NDNX = true");

    if (ndndId_.size() == 0) {
      // First fetch the ndndId of the connected hub.
      NdndIdFetcher fetcher
        (ptr_lib::shared_ptr<NdndIdFetcher::Info>(new NdndIdFetcher::Info
          (this, registeredPrefixId, prefix, onInterest, onRegisterFailed,
           flags, wireFormat, face)));
      // We send the interest using the given wire format so that the hub receives (and sends) in the application's desired wire format.
      // It is OK for func_lib::function make a copy of the function object because the Info is in a ptr_lib::shared_ptr.
      expressInterest
        (getNextEntryId(), ndndIdFetcherInterest_, fetcher, fetcher, wireFormat,
         face);
    }
    else
      registerPrefixHelper
        (registeredPrefixId, ptr_lib::make_shared<const Name>(prefix),
         onInterest, onRegisterFailed, flags, wireFormat, face);
  }
  else
    // The application set the KeyChain for signing NFD interests.
    nfdRegisterPrefix
      (registeredPrefixId, ptr_lib::make_shared<const Name>(prefix), onInterest,
       onRegisterFailed, flags, commandKeyChain, commandCertificateName,
       wireFormat, face);
}

void
Node::removeRegisteredPrefix(uint64_t registeredPrefixId)
{
  int count = 0;
  // Go backwards through the list so we can erase entries.
  // Remove all entries even though registeredPrefixId should be unique.
  for (int i = (int)registeredPrefixTable_.size() - 1; i >= 0; --i) {
    RegisteredPrefix& entry = *registeredPrefixTable_[i];

    if (entry.getRegisteredPrefixId() == registeredPrefixId) {
      ++count;

      if (entry.getRelatedInterestFilterId() > 0)
        // Remove the related interest filter.
        unsetInterestFilter(entry.getRelatedInterestFilterId());

      registeredPrefixTable_.erase(registeredPrefixTable_.begin() + i);
    }
  }

  if (count == 0)
    _LOG_DEBUG("removeRegisteredPrefix: Didn't find registeredPrefixId " << registeredPrefixId);
}

void
Node::setInterestFilter
  (uint64_t interestFilterId, const InterestFilter& filter,
   const OnInterestCallback& onInterest, Face* face)
{
  interestFilterTable_.push_back(ptr_lib::make_shared<InterestFilterEntry>
    (interestFilterId, ptr_lib::make_shared<const InterestFilter>(filter),
     onInterest, face));
}

void
Node::unsetInterestFilter(uint64_t interestFilterId)
{
  int count = 0;
  // Go backwards through the list so we can erase entries.
  // Remove all entries even though interestFilterId should be unique.
  for (int i = (int)interestFilterTable_.size() - 1; i >= 0; --i) {
    if (interestFilterTable_[i]->getInterestFilterId() == interestFilterId) {
      ++count;
      interestFilterTable_.erase(interestFilterTable_.begin() + i);
    }
  }

  if (count == 0)
    _LOG_DEBUG("unsetInterestFilter: Didn't find interestFilterId " << interestFilterId);
}

void
Node::putData(const Data& data, WireFormat& wireFormat)
{
  Blob encoding = data.wireEncode(wireFormat);
  if (encoding.size() > getMaxNdnPacketSize())
    throw runtime_error
      ("The encoded Data packet size exceeds the maximum limit getMaxNdnPacketSize()");

  transport_->send(*encoding);
}

void
Node::send(const uint8_t *encoding, size_t encodingLength)
{
  if (encodingLength > getMaxNdnPacketSize())
    throw runtime_error
      ("The encoded packet size exceeds the maximum limit getMaxNdnPacketSize()");

  transport_->send(encoding, encodingLength);
}

uint64_t
Node::getNextEntryId()
{
  // This is an atomic_uint64_t, so increment is thread-safe.
  return ++lastEntryId_;
}

void
Node::NdndIdFetcher::operator()(const ptr_lib::shared_ptr<const Interest>& interest, const ptr_lib::shared_ptr<Data>& ndndIdData)
{
  // Assume that the content is a DER encoded public key of the ndnd.  Do a quick check that the first byte is for DER encoding.
  if (ndndIdData->getContent().size() < 1 ||
      ndndIdData->getContent().buf()[0] != 0x30) {
    _LOG_DEBUG
      ("Register prefix failed: The content returned when fetching the NDNx ID does not appear to be a public key");
    info_->onRegisterFailed_(info_->prefix_);
    return;
  }

  // Get the digest of the public key.
  uint8_t digest[SHA256_DIGEST_LENGTH];
  ndn_digestSha256(ndndIdData->getContent().buf(), ndndIdData->getContent().size(), digest);

  // Set the ndndId_ and continue.
  // TODO: If there are multiple connected hubs, the NDN ID is really stored per connected hub.
  info_->node_.ndndId_ = Blob(digest, sizeof(digest));
  info_->node_.registerPrefixHelper
    (info_->registeredPrefixId_, info_->prefix_, info_->onInterest_,
     info_->onRegisterFailed_, info_->flags_, info_->wireFormat_, info_->face_);
}

void
Node::NdndIdFetcher::operator()(const ptr_lib::shared_ptr<const Interest>& timedOutInterest)
{
  _LOG_DEBUG("Register prefix failed: Timeout fetching the NDNx ID");
  info_->onRegisterFailed_(info_->prefix_);
}

void
Node::RegisterResponse::operator()(const ptr_lib::shared_ptr<const Interest>& interest, const ptr_lib::shared_ptr<Data>& responseData)
{
  if (info_->isNfdCommand_) {
    // Decode responseData->getContent() and check for a success code.
    // TODO: Move this into the TLV code.
    struct ndn_TlvDecoder decoder;
    ndn_TlvDecoder_initialize
      (&decoder, responseData->getContent().buf(),
       responseData->getContent().size());
    ndn_Error error;
    size_t endOffset;
    if ((error = ndn_TlvDecoder_readNestedTlvsStart
        (&decoder, ndn_Tlv_NfdCommand_ControlResponse, &endOffset))) {
      _LOG_DEBUG
        ("Register prefix failed: Error decoding the NFD response: " <<
         ndn_getErrorString(error));
      info_->onRegisterFailed_(info_->prefix_);
      return;
    }
    uint64_t statusCode;
    if ((error = ndn_TlvDecoder_readNonNegativeIntegerTlv
         (&decoder, ndn_Tlv_NfdCommand_StatusCode, &statusCode))) {
      _LOG_DEBUG
        ("Register prefix failed: Error decoding the NFD response: " <<
         ndn_getErrorString(error));
      info_->onRegisterFailed_(info_->prefix_);
      return;
    }

    // Status code 200 is "OK".
    if (statusCode != 200) {
      _LOG_DEBUG
        ("Register prefix failed: Expected NFD status code 200, got: " <<
         statusCode);
      info_->onRegisterFailed_(info_->prefix_);
      return;
    }

    _LOG_DEBUG("Register prefix succeeded with the NFD forwarder for prefix " <<
               info_->prefix_->toUri());
  }
  else {
    Name expectedName("/ndnx/.../selfreg");
    // Got a response. Do a quick check of expected name components.
    if (responseData->getName().size() < 4 ||
        responseData->getName()[0] != expectedName[0] ||
        responseData->getName()[2] != expectedName[2]) {
      _LOG_DEBUG
        ("Register prefix failed: Unexpected name in NDNx response: " <<
         responseData->getName().toUri());
      info_->onRegisterFailed_(info_->prefix_);
      return;
    }

    _LOG_DEBUG("Register prefix succeeded with the NDNx forwarder for prefix " <<
               info_->prefix_->toUri());
  }
}

void
Node::RegisterResponse::operator()(const ptr_lib::shared_ptr<const Interest>& timedOutInterest)
{
  if (info_->isNfdCommand_) {
    _LOG_DEBUG
      ("Timeout for NFD register prefix command. Attempting an NDNx command...");
    // The application set the commandKeyChain, but we may be connected to NDNx.
    if (info_->node_.ndndId_.size() == 0) {
      // First fetch the ndndId of the connected hub.
      // Pass 0 for registeredPrefixId since the entry was already added to
      //   registeredPrefixTable_ on the first try.
      NdndIdFetcher fetcher
        (ptr_lib::shared_ptr<NdndIdFetcher::Info>(new NdndIdFetcher::Info
          (&info_->node_, 0, *info_->prefix_, info_->onInterest_,
           info_->onRegisterFailed_, info_->flags_, info_->wireFormat_, info_->face_)));
      // We send the interest using the given wire format so that the hub receives (and sends) in the application's desired wire format.
      // It is OK for func_lib::function make a copy of the function object because the Info is in a ptr_lib::shared_ptr.
      info_->node_.expressInterest
        (info_->node_.getNextEntryId(), info_->node_.ndndIdFetcherInterest_,
         fetcher, fetcher, info_->wireFormat_, info_->face_);
    }
    else
      // Pass 0 for registeredPrefixId since the entry was already added to
      //   registeredPrefixTable_ on the first try.
      info_->node_.registerPrefixHelper
        (0, info_->prefix_, info_->onInterest_, info_->onRegisterFailed_,
         info_->flags_, info_->wireFormat_, info_->face_);
  }
  else {
    // An NDNx command was sent because there is no commandKeyChain, so we
    //   can't try an NFD command. Or it was sent from this callback after
    //   trying an NFD command. Fail.
    _LOG_DEBUG
      ("Register prefix failed: Timeout waiting for the response from the register prefix interest");
    info_->onRegisterFailed_(info_->prefix_);
  }
}

void
Node::registerPrefixHelper
  (uint64_t registeredPrefixId, const ptr_lib::shared_ptr<const Name>& prefix,
   const OnInterestCallback& onInterest, const OnRegisterFailed& onRegisterFailed,
   const ForwardingFlags& flags, WireFormat& wireFormat, Face* face)
{
  if (!WireFormat::ENABLE_NDNX)
    // We can get here if the command signing info is set, but running NDNx.
    throw runtime_error
      ("registerPrefix with NDNx is deprecated. To enable while you upgrade your code to use NFD, set WireFormat::ENABLE_NDNX = true");

  // Create a ForwardingEntry.
  // Note: ndnd ignores any freshness that is larger than 3600 seconds and sets 300 seconds instead.  To register "forever",
  //   (=2000000000 sec), the freshness period must be omitted.
  ForwardingEntry forwardingEntry("selfreg", *prefix, PublisherPublicKeyDigest(), -1, flags, -1);
  // Always encode as BinaryXml since the internals of ndnd expect it.
  Blob content = forwardingEntry.wireEncode(*BinaryXmlWireFormat::get());

  // Set the ForwardingEntry as the content of a Data packet and sign.
  Data data;
  data.setContent(content);
  // Use the deprecated setTimestampMilliseconds because ndnd requires it.
  data.getMetaInfo().setTimestampMilliseconds(time(NULL) * 1000.0);
  // For now, self sign with an arbirary key.  In the future, we may not require a signature to register.
  // Always encode as BinaryXml since the internals of ndnd expect it.
  selfregSign(data, *BinaryXmlWireFormat::get());
  Blob encodedData = data.wireEncode(*BinaryXmlWireFormat::get());

  // Create an interest where the name has the encoded Data packet.
  Name interestName;
  const uint8_t component0[] = "ndnx";
  const uint8_t component2[] = "selfreg";
  interestName.append(component0, sizeof(component0) - 1);
  interestName.append(ndndId_);
  interestName.append(component2, sizeof(component2) - 1);
  interestName.append(encodedData);

  Interest interest(interestName);
  interest.setInterestLifetimeMilliseconds(4000.0);
  interest.setScope(1);

  if (registeredPrefixId != 0) {
    uint64_t interestFilterId = 0;
    if (onInterest) {
      // registerPrefix was called with the "combined" form that includes the
      // callback, so add an InterestFilterEntry.
      interestFilterId = getNextEntryId();
      setInterestFilter
        (interestFilterId, InterestFilter(*prefix), onInterest, face);
    }

    registeredPrefixTable_.push_back
      (ptr_lib::shared_ptr<RegisteredPrefix>
       (new RegisteredPrefix(registeredPrefixId, prefix, interestFilterId)));
  }

  // Send the registration interest.
  RegisterResponse response
    (ptr_lib::shared_ptr<RegisterResponse::Info>(new RegisterResponse::Info
     (this, prefix, onInterest, onRegisterFailed, flags, wireFormat, false, face)));
  // It is OK for func_lib::function make a copy of the function object because
  //   the Info is in a ptr_lib::shared_ptr.
  expressInterest
    (getNextEntryId(), interest, response, response, wireFormat, face);
}

void
Node::nfdRegisterPrefix
  (uint64_t registeredPrefixId, const ptr_lib::shared_ptr<const Name>& prefix,
   const OnInterestCallback& onInterest, const OnRegisterFailed& onRegisterFailed,
   const ForwardingFlags& flags, KeyChain& commandKeyChain,
   const Name& commandCertificateName, WireFormat& wireFormat, Face* face)
{
  if (!&commandKeyChain)
    throw runtime_error
      ("registerPrefix: The command KeyChain has not been set. You must call setCommandSigningInfo.");
  if (commandCertificateName.size() == 0)
    throw runtime_error
      ("registerPrefix: The command certificate name has not been set. You must call setCommandSigningInfo.");

  ControlParameters controlParameters;
  controlParameters.setName(*prefix);
  controlParameters.setForwardingFlags(flags);

  Interest commandInterest;
  if (isLocal()) {
    commandInterest.setName(Name("/localhost/nfd/rib/register"));
    // The interest is answered by the local host, so set a short timeout.
    commandInterest.setInterestLifetimeMilliseconds(2000.0);
  }
  else {
    commandInterest.setName(Name("/localhop/nfd/rib/register"));
    // The host is remote, so set a longer timeout.
    commandInterest.setInterestLifetimeMilliseconds(4000.0);
  }
  // NFD only accepts TlvWireFormat packets.
  commandInterest.getName().append(controlParameters.wireEncode(*TlvWireFormat::get()));
  makeCommandInterest
    (commandInterest, commandKeyChain, commandCertificateName,
     *TlvWireFormat::get());

  if (registeredPrefixId != 0) {
    uint64_t interestFilterId = 0;
    if (onInterest) {
      // registerPrefix was called with the "combined" form that includes the
      // callback, so add an InterestFilterEntry.
      interestFilterId = getNextEntryId();
      setInterestFilter
        (interestFilterId, InterestFilter(*prefix), onInterest, face);
    }

    registeredPrefixTable_.push_back
      (ptr_lib::shared_ptr<RegisteredPrefix>(new RegisteredPrefix
        (registeredPrefixId, prefix, interestFilterId)));
  }

  // Send the registration interest.
  RegisterResponse response
    (ptr_lib::shared_ptr<RegisterResponse::Info>(new RegisterResponse::Info
     (this, prefix, onInterest, onRegisterFailed, flags, wireFormat, true, face)));
  // It is OK for func_lib::function make a copy of the function object because
  //   the Info is in a ptr_lib::shared_ptr.
  expressInterest
    (getNextEntryId(), commandInterest, response, response, wireFormat, face);
}

void
Node::processEvents()
{
  transport_->processEvents();

  // Check for delayed calls. Since callLater does a sorted insert into
  // delayedCallTable_, the check for timeouts is quick and does not require
  // searching the entire table. If callLater is overridden to use a different
  // mechanism, then processEvents is not needed to check for delayed calls.
  MillisecondsSince1970 now = ndn_getNowMilliseconds();
  // delayedCallTable_ is sorted on _callTime, so we only need to process
  // the timed-out entries at the front, then quit.
  while (delayedCallTable_.size() > 0 &&
         delayedCallTable_.front()->getCallTime() <= now) {
    ptr_lib::shared_ptr<DelayedCall> delayedCall = delayedCallTable_.front();
    delayedCallTable_.erase(delayedCallTable_.begin());
    delayedCall->callCallback();
  }
}

void
Node::onReceivedElement(const uint8_t *element, size_t elementLength)
{
  // First, decode as Interest or Data.
  ptr_lib::shared_ptr<Interest> interest;
  ptr_lib::shared_ptr<Data> data;
  // The type codes for TLV Interest and Data packets are chosen to not
  //   conflict with the first byte of a binary XML packet, so we can
  //   just look at the first byte.
  if (element[0] == ndn_Tlv_Interest || element[0] == ndn_Tlv_Data) {
    TlvDecoder decoder(element, elementLength);
    if (decoder.peekType(ndn_Tlv_Interest, elementLength)) {
      interest.reset(new Interest());
      interest->wireDecode(element, elementLength, *TlvWireFormat::get());
    }
    else if (decoder.peekType(ndn_Tlv_Data, elementLength)) {
      data.reset(new Data());
      data->wireDecode(element, elementLength, *TlvWireFormat::get());
    }
  }
  else {
    // Binary XML.
    BinaryXmlDecoder decoder(element, elementLength);
    if (decoder.peekDTag(ndn_BinaryXml_DTag_Interest)) {
      interest.reset(new Interest());
      interest->wireDecode(element, elementLength, *BinaryXmlWireFormat::get());
    }
    else if (decoder.peekDTag(ndn_BinaryXml_DTag_ContentObject)) {
      data.reset(new Data());
      data->wireDecode(element, elementLength, *BinaryXmlWireFormat::get());
    }
  }

  // Now process as Interest or Data.
  if (interest) {
    // Call all interest filter callbacks which match.
    for (size_t i = 0; i < interestFilterTable_.size(); ++i) {
      InterestFilterEntry &entry = *interestFilterTable_[i];
      if (entry.getFilter()->doesMatch(interest->getName()))
        entry.getOnInterest()
          (entry.getPrefix(), interest, entry.getFace(),
           entry.getInterestFilterId(), entry.getFilter());
    }
  }
  else if (data) {
    vector<ptr_lib::shared_ptr<PendingInterest> > pitEntries;
    extractEntriesForExpressedInterest(data->getName(), pitEntries);
    for (size_t i = 0; i < pitEntries.size(); ++i)
      pitEntries[i]->getOnData()(pitEntries[i]->getInterest(), data);
  }
}

void
Node::shutdown()
{
  transport_->close();
}

void
Node::expressInterestHelper
  (uint64_t pendingInterestId,
   const ptr_lib::shared_ptr<const Interest>& interestCopy,
   const OnData& onData, const OnTimeout& onTimeout, WireFormat& wireFormat,
   Face* face)
{
  ptr_lib::shared_ptr<PendingInterest> pendingInterest(new PendingInterest
    (pendingInterestId, interestCopy, onData, onTimeout));
  pendingInterestTable_.push_back(pendingInterest);
  if (interestCopy->getInterestLifetimeMilliseconds() >= 0.0)
    // Set up the timeout.
    face->callLater
      (interestCopy->getInterestLifetimeMilliseconds(),
       bind(&Node::processInterestTimeout, this, pendingInterest));

  // Special case: For timeoutPrefix_ we don't actually send the interest.
  if (!timeoutPrefix_.match(interestCopy->getName())) {
    Blob encoding = interestCopy->wireEncode(wireFormat);
    if (encoding.size() > getMaxNdnPacketSize())
      throw runtime_error
        ("The encoded interest size exceeds the maximum limit getMaxNdnPacketSize()");
    transport_->send(*encoding);
  }
}

void
Node::processInterestTimeout(ptr_lib::shared_ptr<PendingInterest> pendingInterest)
{
  if (pendingInterest->getIsRemoved())
    // extractEntriesForExpressedInterest or removePendingInterest has removed 
    // pendingInterest from pendingInterestTable_, so we don't need to look for
    // it. Do nothing.
    return;

  // Find the entry.
  for (vector<ptr_lib::shared_ptr<PendingInterest> >::iterator entry =
         pendingInterestTable_.begin();
       entry != pendingInterestTable_.end();
       ++entry) {
    if (entry->get() == pendingInterest.get()) {
      pendingInterestTable_.erase(entry);
      pendingInterest->callTimeout();
      return;
    }
  }

  // The pending interest has been removed. Do nothing.
}

void
Node::extractEntriesForExpressedInterest
  (const Name& name, vector<ptr_lib::shared_ptr<PendingInterest> > &entries)
{
  // Go backwards through the list so we can erase entries.
  for (int i = (int)pendingInterestTable_.size() - 1; i >= 0; --i) {
    if (pendingInterestTable_[i]->getInterest()->matchesName(name)) {
      entries.push_back(pendingInterestTable_[i]);
      // We let the callback from callLater call _processInterestTimeout, but
      // for efficiency, mark this as removed so that it returns right away.
      pendingInterestTable_[i]->setIsRemoved();
      pendingInterestTable_.erase(pendingInterestTable_.begin() + i);
    }
  }
}

void
Node::callLater(Milliseconds delayMilliseconds, const Face::Callback& callback)
{
  ptr_lib::shared_ptr<DelayedCall> delayedCall
    (new DelayedCall(delayMilliseconds, callback));
  // Insert into delayedCallTable_, sorted on delayedCall.getCallTime().
  delayedCallTable_.insert
    (std::lower_bound(delayedCallTable_.begin(), delayedCallTable_.end(),
                      delayedCall, delayedCallCompare_),
     delayedCall);
}

Node::DelayedCall::DelayedCall
  (Milliseconds delayMilliseconds, const Face::Callback& callback)
  : callback_(callback),
    callTime_(ndn_getNowMilliseconds() + delayMilliseconds)
{
}

Node::PendingInterest::PendingInterest
  (uint64_t pendingInterestId, const ptr_lib::shared_ptr<const Interest>& interest, const OnData& onData, const OnTimeout& onTimeout)
: pendingInterestId_(pendingInterestId), interest_(interest), onData_(onData), onTimeout_(onTimeout)
{
}

void
Node::PendingInterest::callTimeout()
{
  if (onTimeout_) {
    // Ignore all exceptions.
    try {
      onTimeout_(interest_);
    }
    catch (...) { }
  }
}

}
