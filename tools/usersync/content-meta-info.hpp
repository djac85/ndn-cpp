/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil -*- */
/**
 * Copyright (C) 2017 Regents of the University of California.
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

#ifndef NDN_CONTENT_META_INFO_HPP
#define NDN_CONTENT_META_INFO_HPP

#include <ndn-cpp/util/blob.hpp>

namespace ndn {

class ContentMetaInfo {
public:
  /**
   * Create a ContentMetaInfo where all the fields have default unspecified values.
   */
  ContentMetaInfo()
  {
    clear();
  }

  /**
   * Get the content type.
   * @return The content type. If not specified, return an empty string.
   */
  const std::string&
  getContentType() const { return contentType_; }

  /**
   * Get the time stamp.
   * @return The time stamp as milliseconds since Jan 1, 1970 UTC. If not
   * specified, return -1.
   */
  MillisecondsSince1970
  getTimestamp() const { return timestamp_; }

  /**
   * Get the content size.
   * @return The content size. If not specified, return -1.
   */
  int
  getContentSize() const { return contentSize_; }

  /**
   * Get the Blob containing the optional other info.
   * @return The other info. If not specified, return an isNull Blob.
   */
  const Blob&
  getOther() const { return other_; }

  /**
   * Set the content type.
   * @param contentType The content type.
   * @return This ContentMetaInfo so that you can chain calls to update values.
   */
  ContentMetaInfo&
  setContentType(const std::string& contentType)
  {
    contentType_ = contentType;
    return *this;
  }

  /**
   * Set the time stamp.
   * @param timestamp The time stamp.
   * @return This ContentMetaInfo so that you can chain calls to update values.
   */
  ContentMetaInfo&
  setTimestamp(MillisecondsSince1970 timestamp)
  {
    timestamp_ = timestamp;
    return *this;
  }

  /**
   * Set the content size.
   * @param contentSize The content size.
   * @return This ContentMetaInfo so that you can chain calls to update values.
   */
  ContentMetaInfo&
  setContentSize(int contentSize)
  {
    contentSize_ = contentSize;
    return *this;
  }

  /**
   * Set the Blob containing the optional other info.
   * @param other The other info, or a default null Blob() if not specified.
   * @return This ContentMetaInfo so that you can chain calls to update values.
   */
  ContentMetaInfo&
  setOther(const Blob& other)
  {
    other_ = other;
    return *this;
  }

  /**
   * Set all the fields to their default unspecified values.
   */
  void
  clear();

  /**
   * Encode this ContentMetaInfo.
   * @return The encoded byte array.
   */
  Blob
  wireEncode() const;

  /**
   * Decode the input and update this ContentMetaInfo.
   * @param input The input byte array to be decoded.
   * @param inputLength The length of input.
   */
  void
  wireDecode(const uint8_t *input, size_t inputLength);

  /**
   * Decode the input and update this Schedule.
   * @param input The input byte array to be decoded.
   */
  void
  wireDecode(const std::vector<uint8_t>& input)
  {
    wireDecode(&input[0], input.size());
  }

  /**
   * Decode the input and update this Schedule.
   * @param input The input byte array to be decoded as an immutable Blob.
   */
  void
  wireDecode(const Blob& input)
  {
    wireDecode(input.buf(), input.size());
  }

private:
  std::string contentType_;
  MillisecondsSince1970 timestamp_;
  int contentSize_;
  Blob other_;
};

}

#endif
