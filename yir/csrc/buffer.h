/*
 *  Copyright 2020 Xilinx Inc.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#pragma once

#include <sys/types.h>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace yir {

struct Buffer {
  void* data;
  ssize_t itemsize;
  std::string format;
  ssize_t ndim;
  std::vector<ssize_t> shape;
  std::vector<ssize_t> strides;

  ssize_t size;
  bool own_data;

  Buffer(const Buffer& xb)
      : itemsize(xb.itemsize),
        format(xb.format),
        ndim(xb.ndim),
        shape(xb.shape),
        strides(xb.strides),
        size(xb.size),
        own_data(true) {
    // TODO: transfer data instead of copying
    // data = xb.data;
    // xb.own_data = false;
    data = ::operator new(size* itemsize);
    memcpy(data, xb.data, size * itemsize);
  }

  Buffer(Buffer&& xb)
      : itemsize(xb.itemsize),
        format(xb.format),
        ndim(xb.ndim),
        shape(xb.shape),
        strides(xb.strides),
        size(xb.size),
        own_data(xb.own_data) {
    // If xb owns the data that is being moved, then we transfer ownership
    //  to this object
    if (xb.own_data)
      xb.disable_data_ownership();
    data = xb.data;
  }

  Buffer(
      void* data_,
      ssize_t itemsize_,
      std::string format_,
      ssize_t ndim_,
      std::vector<ssize_t> shape_,
      std::vector<ssize_t> strides_,
      bool copy = true,
      bool own_data_ = true)
      : data(data_),
        itemsize(itemsize_),
        format(format_),
        ndim(ndim_),
        shape(shape_),
        strides(strides_),
        size(1),
        own_data(own_data_) {
    // std::cout << "Initialize Buffer " << this << std::endl;
    if (ndim != (ssize_t)shape.size() || ndim != (ssize_t)strides.size())
      throw std::invalid_argument(
          "Buffer: ndim doesn't match shape and/or"
          " strides length");
    for (ssize_t i = 0; i < (ssize_t)ndim; ++i)
      size *= shape[i];
    if (size < 0)
      size *= -1;

    if (copy) {
      data = ::operator new(size* itemsize);
      memcpy(data, data_, size * itemsize);
    } else {
      data = data_;
    }
  }

  Buffer(
      void* data_,
      ssize_t itemsize_,
      std::string format_,
      ssize_t ndim_,
      std::vector<ssize_t> shape_,
      bool copy = true,
      bool own_data_ = true)
      : data(data_),
        itemsize(itemsize_),
        format(format_),
        ndim(ndim_),
        shape(shape_),
        size(1),
        own_data(own_data_) {
    if (ndim != (ssize_t)shape.size()) // || ndim != (ssize_t) strides.size()
      throw std::invalid_argument(
          "Buffer: ndim doesn't match shape and/or"
          " strides length");
    for (ssize_t i = 0; i < (ssize_t)ndim; ++i)
      size *= shape[i];
    if (size < 0)
      size *= -1;

    ssize_t nxt_stride = (size > 0) ? size * itemsize : itemsize;
    strides.push_back(nxt_stride / shape[0]);
    nxt_stride /= shape[0];
    for (ssize_t i = 1; i < (ssize_t)ndim; ++i) {
      nxt_stride /= shape[i];
      strides.push_back(nxt_stride);
    }

    if (copy) {
      data = ::operator new(size* itemsize);
      memcpy(data, data_, size * itemsize);
    } else {
      data = data_;
    }
  }

  Buffer& operator=(const Buffer& xb) {
    if (&xb == this)
      return *this;
    // Release data if we are owning it
    if (own_data)
      ::operator delete(data);
    itemsize = xb.itemsize;
    format = xb.format;
    ndim = xb.ndim;
    shape = xb.shape;
    strides = xb.strides;
    size = xb.size;
    own_data = true;
    data = ::operator new(size* itemsize);
    memcpy(data, xb.data, size * itemsize);
    return *this;
  }

  Buffer& operator=(Buffer&& xb) {
    if (&xb == this)
      return *this;
    // Release data if we are owning it
    if (own_data)
      ::operator delete(data);
    itemsize = xb.itemsize;
    format = xb.format;
    ndim = xb.ndim;
    shape = xb.shape;
    strides = xb.strides;
    size = xb.size;
    own_data = xb.own_data;
    // If xb owns the data that is being moved, then we transfer ownership
    //  to this object
    if (xb.own_data)
      xb.disable_data_ownership();
    data = xb.data;
    return *this;
  }

  void enable_data_ownership() {
    own_data = true;
  }

  void disable_data_ownership() {
    own_data = false;
  }

  ~Buffer() {
    if (own_data)
      ::operator delete(data);
  }
};

typedef std::shared_ptr<Buffer> BufferHolder;

inline BufferHolder create_buffer(std::vector<ssize_t>& shape) {
  int64_t size = 1;
  std::vector<ssize_t> buffer_shape;
  for (const int64_t& e : shape) {
    size *= e;
    buffer_shape.push_back(e);
  }
  if (size < 0)
    size *= -1;
  void* input_data = malloc(4 * size);
  return std::shared_ptr<Buffer>(
      new Buffer(input_data, 4, "f", buffer_shape.size(), shape, false, true));
}

} // namespace yir
