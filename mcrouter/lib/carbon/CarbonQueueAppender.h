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

#include <sys/uio.h>

#include <type_traits>
#include <utility>

#include <folly/Bits.h>
#include <folly/io/IOBuf.h>
#include <folly/Varint.h>

namespace folly {
class IOBuf;
} // folly

namespace carbon {

class CarbonQueueAppenderStorage {
 public:
  CarbonQueueAppenderStorage() {
    reset();
  }

  CarbonQueueAppenderStorage(const CarbonQueueAppenderStorage&) = delete;
  CarbonQueueAppenderStorage& operator=(const CarbonQueueAppenderStorage&) =
      delete;

  void append(const folly::IOBuf& buf);

  void push(const uint8_t* buf, size_t len);

  void coalesce();

  void reset() {
    storageIdx_ = 0;
    head_ = folly::IOBuf();
    // Reserve first element of iovs_ for header, which won't be filled in
    // until after body data is serialized.
    iovs_[0] = {headerBuf_, 0};
    nIovsUsed_ = 1;
    canUsePreviousIov_ = false;
  }

  std::pair<const struct iovec*, size_t> getIovecs() const {
    return std::make_pair(iovs_, nIovsUsed_);
  }

  size_t computeBodySize() const {
    size_t bodySize = 0;
    // Skip iovs_[0], which refers to message header
    for (size_t i = 1; i < nIovsUsed_; ++i) {
      bodySize += iovs_[i].iov_len;
    }
    return bodySize;
  }

  // Hack: we expose headerBuf_ so users can write directly to it.
  // It is the responsibility of the user to report how much data was written
  // via reportHeaderSize().
  uint8_t* getHeaderBuf() {
    assert(iovs_[0].iov_base == headerBuf_);
    return headerBuf_;
  }

  void reportHeaderSize(size_t headerSize) {
    iovs_[0].iov_len = headerSize;
  }

 private:
  static constexpr size_t kMaxIovecs{32};

  // Copied from UmbrellaProtocol.h, which will eventually die
  static constexpr size_t kMaxAdditionalFields = 3;
  static constexpr size_t kMaxHeaderLength = 1 /* magic byte */ +
      1 /* GroupVarint header (lengths of 4 ints) */ +
      4 * sizeof(uint32_t) /* body size, typeId, reqId, num addl fields */ +
      2 * kMaxAdditionalFields *
          folly::kMaxVarintLength64; /* key and value for additional fields */

  size_t storageIdx_{0};
  size_t nIovsUsed_{0};
  bool canUsePreviousIov_{false};

  // For safety reasons, we use another buffer for the header data
  uint8_t headerBuf_[kMaxHeaderLength];
  // Buffer used for non-IOBuf data, e.g., ints, strings, and protocol
  // data
  uint8_t storage_[512];

  // The first iovec in iovs_ points to Caret message header data, and nothing
  // else. The remaining iovecs are used for the message body. Note that we do
  // not share iovs_[0] with body data, even if it would be possible, e.g., we
  // do not append the CT_STRUCT (struct beginning delimiter) to iovs_[0].
  struct iovec iovs_[kMaxIovecs];

  // Chain of IOBufs used for IOBuf fields, like key and value. Note that we
  // also maintain views into this data via iovs_.
  folly::IOBuf head_;
};


/**
 * Mcrouter's own implementation of folly's QueueAppender.  CarbonQueueAppender
 * implements the portion of the folly::io::QueueAppender interface needed by
 * apache::thrift::CompactProtocolWriter.
 * We have our own version of QueueAppender in order to support more efficient
 * memory management for mcrouter's use case.
 */
class CarbonQueueAppender {
 public:
  CarbonQueueAppender(CarbonQueueAppenderStorage* storage, uint64_t unused) {
    reset(storage, unused);
  }

  template <class T>
  typename std::enable_if<std::is_arithmetic<T>::value>::type
  write(T value) {
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&value);
    push(bytes, sizeof(T));
  }

  template <class T>
  void writeBE(T value) {
    write(folly::Endian::big(value));
  }

  template <class T>
  void writeLE(T value) {
    write(folly::Endian::little(value));
  }

  void reset(CarbonQueueAppenderStorage* storage, uint64_t /* unused */) {
    storage_ = storage;
  }

  // The user is responsible for ensuring storage_ is valid before calling
  // push() or insert()
  void push(const uint8_t* buf, size_t len) {
    assert(storage_);
    storage_->push(buf, len);
  }

  void insert(std::unique_ptr<folly::IOBuf> buf) {
    assert(storage_);
    assert(buf);
    storage_->append(*buf);
  }

  void insert(const folly::IOBuf& buf) {
    assert(storage_);
    storage_->append(buf);
  }

 private:
  CarbonQueueAppenderStorage* storage_{nullptr};
};

} // carbon
