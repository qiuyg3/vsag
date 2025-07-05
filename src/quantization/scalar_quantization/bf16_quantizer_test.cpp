
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

#include "bf16_quantizer.h"

#include <catch2/catch_test_macros.hpp>
#include <vector>

#include "default_allocator.h"
#include "fixtures.h"
#include "quantization/quantizer_test.h"
#include "safe_allocator.h"

using namespace vsag;

const auto dims = fixtures::get_common_used_dims(3, 225);
const auto counts = {10, 101};

template <MetricType metric>
void
TestQuantizerEncodeDecodeMetricBF16(uint64_t dim, int count, float error = 1e-3) {
    auto allocator = SafeAllocator::FactoryDefaultAllocator();
    BF16Quantizer<metric> quantizer(dim, allocator.get());
    TestQuantizerEncodeDecode(quantizer, dim, count, error);
    TestQuantizerEncodeDecodeSame(quantizer, dim, count, 65536, error);
}

TEST_CASE("BF16 Encode and Decode", "[ut][BF16Quantizer]") {
    constexpr MetricType metrics[2] = {MetricType::METRIC_TYPE_L2SQR, MetricType::METRIC_TYPE_IP};
    float error = 6e-3F;
    for (auto dim : dims) {
        for (auto count : counts) {
            TestQuantizerEncodeDecodeMetricBF16<metrics[0]>(dim, count, error);
            TestQuantizerEncodeDecodeMetricBF16<metrics[1]>(dim, count, error);
        }
    }
}

template <MetricType metric>
void
TestComputeMetricBF16(uint64_t dim, int count, float error = 1e-5) {
    auto allocator = SafeAllocator::FactoryDefaultAllocator();
    BF16Quantizer<metric> quantizer(dim, allocator.get());
    TestComputeCodes<BF16Quantizer<metric>, metric>(quantizer, dim, count, error);
    TestComputer<BF16Quantizer<metric>, metric>(quantizer, dim, count, error);
}

TEST_CASE("BF16 Compute", "[ut][BF16Quantizer]") {
    constexpr MetricType metrics[3] = {
        MetricType::METRIC_TYPE_L2SQR, MetricType::METRIC_TYPE_COSINE, MetricType::METRIC_TYPE_IP};
    float error = 6e-3F;
    for (auto dim : dims) {
        for (auto count : counts) {
            TestComputeMetricBF16<metrics[0]>(dim, count, error);
            TestComputeMetricBF16<metrics[1]>(dim, count, error);
            TestComputeMetricBF16<metrics[2]>(dim, count, error);
        }
    }
}

template <MetricType metric>
void
TestSerializeAndDeserializeMetricBF16(uint64_t dim, int count, float error = 1e-5) {
    auto allocator = SafeAllocator::FactoryDefaultAllocator();
    BF16Quantizer<metric> quantizer1(dim, allocator.get());
    BF16Quantizer<metric> quantizer2(0, allocator.get());
    TestSerializeAndDeserialize<BF16Quantizer<metric>, metric>(
        quantizer1, quantizer2, dim, count, error);
}

TEST_CASE("BF16 Serialize and Deserialize", "[ut][BF16Quantizer]") {
    constexpr MetricType metrics[3] = {
        MetricType::METRIC_TYPE_L2SQR, MetricType::METRIC_TYPE_COSINE, MetricType::METRIC_TYPE_IP};
    float error = 6e-3F;
    for (auto dim : dims) {
        for (auto count : counts) {
            TestSerializeAndDeserializeMetricBF16<metrics[0]>(dim, count, error);
            TestSerializeAndDeserializeMetricBF16<metrics[1]>(dim, count, error);
            TestSerializeAndDeserializeMetricBF16<metrics[2]>(dim, count, error);
        }
    }
}
