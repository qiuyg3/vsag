
// Copyright 2024-present the vsag project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <cstring>
#include <limits>
#include <vector>

#include "bf16_quantizer_parameter.h"
#include "byte_buffer.h"
#include "index/index_common_param.h"
#include "inner_string_params.h"
#include "quantization/quantizer.h"
#include "simd/bf16_simd.h"
#include "simd/normalize.h"
#include "typing.h"

namespace vsag {

template <MetricType metric = MetricType::METRIC_TYPE_L2SQR>
class BF16Quantizer : public Quantizer<BF16Quantizer<metric>> {
public:
    explicit BF16Quantizer(int dim, Allocator* allocator);

    explicit BF16Quantizer(const BF16QuantizerParamPtr& param,
                           const IndexCommonParam& common_param);

    explicit BF16Quantizer(const QuantizerParamPtr& param, const IndexCommonParam& common_param);

    bool
    TrainImpl(const DataType* data, uint64_t count);

    bool
    EncodeOneImpl(const DataType* data, uint8_t* codes) const;

    bool
    EncodeBatchImpl(const DataType* data, uint8_t* codes, uint64_t count);

    bool
    DecodeOneImpl(const uint8_t* codes, DataType* data);

    bool
    DecodeBatchImpl(const uint8_t* codes, DataType* data, uint64_t count);

    inline float
    ComputeImpl(const uint8_t* codes1, const uint8_t* codes2);

    inline void
    ProcessQueryImpl(const DataType* query, Computer<BF16Quantizer>& computer) const;

    inline void
    ComputeDistImpl(Computer<BF16Quantizer>& computer, const uint8_t* codes, float* dists) const;

    inline void
    ComputeBatchDistImpl(Computer<BF16Quantizer<metric>>& computer,
                         uint64_t count,
                         const uint8_t* codes,
                         float* dists) const;

    inline void
    ReleaseComputerImpl(Computer<BF16Quantizer<metric>>& computer) const;

    inline void
    SerializeImpl(StreamWriter& writer){};

    inline void
    DeserializeImpl(StreamReader& reader){};

    [[nodiscard]] std::string
    NameImpl() const {
        return QUANTIZATION_TYPE_VALUE_BF16;
    }
};

template <MetricType metric>
BF16Quantizer<metric>::BF16Quantizer(int dim, Allocator* allocator)
    : Quantizer<BF16Quantizer<metric>>(dim, allocator) {
    this->code_size_ = dim * 2;
}

template <MetricType metric>
BF16Quantizer<metric>::BF16Quantizer(const BF16QuantizerParamPtr& param,
                                     const IndexCommonParam& common_param)
    : BF16Quantizer<metric>(common_param.dim_, common_param.allocator_.get()){};

template <MetricType metric>
BF16Quantizer<metric>::BF16Quantizer(const QuantizerParamPtr& param,
                                     const IndexCommonParam& common_param)
    : BF16Quantizer<metric>(std::dynamic_pointer_cast<BF16QuantizerParameter>(param),
                            common_param){};

template <MetricType metric>
bool
BF16Quantizer<metric>::TrainImpl(const DataType* data, uint64_t count) {
    if (data == nullptr) {
        return false;
    }
    return true;
}

template <MetricType metric>
bool
BF16Quantizer<metric>::EncodeOneImpl(const DataType* data, uint8_t* codes) const {
    const DataType* cur = data;
    Vector<float> tmp(this->allocator_);
    if constexpr (metric == MetricType::METRIC_TYPE_COSINE) {
        tmp.resize(this->dim_);
        Normalize(data, tmp.data(), this->dim_);
        cur = tmp.data();
    }
    auto* codes_bf16 = reinterpret_cast<uint16_t*>(codes);
    for (int i = 0; i < this->dim_; ++i) {
        codes_bf16[i] = generic::FloatToBF16(cur[i]);
    }

    return true;
}

template <MetricType metric>
bool
BF16Quantizer<metric>::EncodeBatchImpl(const DataType* data, uint8_t* codes, uint64_t count) {
    for (uint64_t i = 0; i < count; ++i) {
        this->EncodeOneImpl(data + i * this->dim_, codes + i * this->code_size_);
    }
    return true;
}

template <MetricType metric>
bool
BF16Quantizer<metric>::DecodeOneImpl(const uint8_t* codes, DataType* data) {
    const auto* codes_bf16 = reinterpret_cast<const uint16_t*>(codes);

    for (uint64_t d = 0; d < this->dim_; d++) {
        data[d] = generic::BF16ToFloat(codes_bf16[d]);
    }
    return true;
}

template <MetricType metric>
bool
BF16Quantizer<metric>::DecodeBatchImpl(const uint8_t* codes, DataType* data, uint64_t count) {
    for (uint64_t i = 0; i < count; ++i) {
        this->DecodeOneImpl(codes + i * this->code_size_, data + i * this->dim_);
    }
    return true;
}

template <MetricType metric>
inline float
BF16Quantizer<metric>::ComputeImpl(const uint8_t* codes1, const uint8_t* codes2) {
    if constexpr (metric == MetricType::METRIC_TYPE_L2SQR) {
        return BF16ComputeL2Sqr(codes1, codes2, this->dim_);
    } else if constexpr (metric == MetricType::METRIC_TYPE_IP or
                         metric == MetricType::METRIC_TYPE_COSINE) {
        return 1 - BF16ComputeIP(codes1, codes2, this->dim_);
    } else {
        return 0;
    }
}

template <MetricType metric>
void
BF16Quantizer<metric>::ProcessQueryImpl(const DataType* query,
                                        Computer<BF16Quantizer>& computer) const {
    try {
        computer.buf_ = reinterpret_cast<uint8_t*>(this->allocator_->Allocate(this->code_size_));
        this->EncodeOneImpl(query, computer.buf_);
    } catch (const std::bad_alloc& e) {
        logger::error("bad alloc when init computer buf");
        throw std::bad_alloc();
    }
}

template <MetricType metric>
void
BF16Quantizer<metric>::ComputeDistImpl(Computer<BF16Quantizer>& computer,
                                       const uint8_t* codes,
                                       float* dists) const {
    auto* buf = computer.buf_;
    if constexpr (metric == MetricType::METRIC_TYPE_L2SQR) {
        dists[0] = BF16ComputeL2Sqr(buf, codes, this->dim_);
    } else if constexpr (metric == MetricType::METRIC_TYPE_IP or
                         metric == MetricType::METRIC_TYPE_COSINE) {
        dists[0] = 1 - BF16ComputeIP(buf, codes, this->dim_);
    } else {
        logger::error("unsupported metric type");
        dists[0] = 0;
    }
}

template <MetricType metric>
void
BF16Quantizer<metric>::ComputeBatchDistImpl(Computer<BF16Quantizer<metric>>& computer,
                                            uint64_t count,
                                            const uint8_t* codes,
                                            float* dists) const {
    // TODO(LHT): Optimize batch for simd
    for (uint64_t i = 0; i < count; ++i) {
        this->ComputeDistImpl(computer, codes + i * this->code_size_, dists + i);
    }
}

template <MetricType metric>
void
BF16Quantizer<metric>::ReleaseComputerImpl(Computer<BF16Quantizer<metric>>& computer) const {
    this->allocator_->Deallocate(computer.buf_);
}

}  // namespace vsag
