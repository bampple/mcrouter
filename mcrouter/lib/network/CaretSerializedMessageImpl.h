/*
 *  Copyright (c) 2016, Facebook, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */
#pragma once

#include <folly/Range.h>
#include <folly/Varint.h>

#include "mcrouter/lib/carbon/CarbonQueueAppender.h"
#include "mcrouter/lib/network/UmbrellaProtocol.h"

namespace facebook {
namespace memcache {

struct CodecIdRange;
class CompressionCodec;
template <class T>
class TypedThriftMessage;
template <class T>
class TypedThriftRequest;

/**
 * Class for serializing requests in the form of thrift structs.
 */
class CaretSerializedMessage {
 public:
  CaretSerializedMessage() = default;

  CaretSerializedMessage(const CaretSerializedMessage&) = delete;
  CaretSerializedMessage& operator=(const CaretSerializedMessage&) = delete;
  CaretSerializedMessage(CaretSerializedMessage&&) noexcept = delete;
  CaretSerializedMessage& operator=(CaretSerializedMessage&&) = delete;

  void clear() {
    storage_.reset();
  }

  /**
   * Prepare requests for serialization for an Operation
   *
   * @param req               Request
   * @param iovOut            Set to the beginning of array of ivecs that
   *                          reference serialized data.
   * @param supportedCodecs   Range of supported compression codecs.
   * @param niovOut           Number of valid iovecs referenced by iovOut.
   *
   * @return true iff message was successfully prepared.
   */
  template <class ThriftType>
  bool prepare(const TypedThriftRequest<ThriftType>& req,
               size_t reqId,
               const CodecIdRange& supportedCodecs,
               const struct iovec*& iovOut,
               size_t& niovOut) noexcept;

  /**
   * Prepare replies for serialization
   *
   * @param  reply    TypedReply
   * @param  codec    Codec to use to compress the reply. If nullptr, reply
   *                  won't be compressed.
   * @param  iovOut   will be set to the beginning of array of ivecs
   * @param  niovOut  number of valid iovecs referenced by iovOut.
   * @return true iff message was successfully prepared.
   */
  template <class ThriftType>
  bool prepare(TypedThriftReply<ThriftType>&& reply,
               size_t reqId,
               CompressionCodec* codec,
               const struct iovec*& iovOut,
               size_t& niovOut) noexcept;

 private:
  carbon::CarbonQueueAppenderStorage storage_;

  template <class ThriftType>
  bool fill(const TypedThriftRequest<ThriftType>& tmsg,
            uint32_t reqId,
            size_t typeId,
            uint64_t traceId,
            const CodecIdRange& supportedCodecs,
            const struct iovec*& iovOut,
            size_t& niovOut);

  template <class ThriftType>
  bool fill(const TypedThriftReply<ThriftType>& tmsg,
            uint32_t reqId,
            size_t typeId,
            uint64_t traceId,
            CompressionCodec* codec,
            const struct iovec*& iovOut,
            size_t& niovOut);

  void fillImpl(UmbrellaMessageInfo& info,
                uint32_t reqId,
                size_t typeId,
                uint64_t traceId,
                const struct iovec*& iovOut,
                size_t& niovOut);

  /**
   * Compress body of message in storage_
   *
   * @param codec             Compression codec to use in compression.
   * @param uncompressedSize  Original (uncompressed) size of the body of the
   *                          message.
   * @return                  True if compression succeeds. Otherwise, false.
   */
  bool maybeCompress(CompressionCodec* codec, size_t uncompressedSize);
};

} // memcache
} // facebook

#include "mcrouter/lib/network/CaretSerializedMessageImpl-inl.h"
