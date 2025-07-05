
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

#include <catch2/catch_test_macros.hpp>
#include <fstream>

#include "fixtures.h"
#include "iostream"
#include "quantizer.h"
#include "simd/normalize.h"
#include "simd/simd.h"

using namespace vsag;

template <typename T>
void
TestEncodeDecodeRaBitQ(Quantizer<T>& quantizer,
                       uint64_t dim,
                       int count,
                       float same_sign_rate = 0.6f) {
    // Generate centroid and data
    assert(count % 2 == 0);
    auto centroid = fixtures::generate_vectors(1, dim, false, 114514);
    std::fill(centroid.begin(), centroid.end(), 0);
    std::vector<float> vecs(dim * count);
    for (int64_t i = 0; i < count; ++i) {
        for (int64_t d = 0; d < dim; ++d) {
            vecs[i * dim + d] = centroid[d] + (i % 2 == 0 ? i + 1 : -i);
        }
    }

    // Init quantizer
    quantizer.ReTrain(vecs.data(), count);

    // Test EncodeOne & DecodeOne
    float count_same_sign_1 = 0;
    std::vector<uint8_t> codes1(quantizer.GetCodeSize() * count);
    for (uint64_t i = 0; i < count; ++i) {
        uint8_t* codes = codes1.data() + i * quantizer.GetCodeSize();
        quantizer.EncodeOne(vecs.data() + i * dim, codes);

        std::vector<float> out_vec(dim);
        quantizer.DecodeOne(codes, out_vec.data());
        for (uint64_t d = 0; d < dim; ++d) {
            if (vecs[i * dim + d] * out_vec[d] >= 0) {
                count_same_sign_1++;
            }
        }
    }
    REQUIRE(count_same_sign_1 / (count * dim) > same_sign_rate);

    // Test EncodeBatch & DecodeBatch
    float count_same_sign_2 = 0;
    std::vector<uint8_t> codes2(quantizer.GetCodeSize() * count);
    quantizer.EncodeBatch(vecs.data(), codes2.data(), count);
    for (int c = 0; c < quantizer.GetCodeSize() * count; c++) {
        REQUIRE(codes1[c] == codes2[c]);
    }

    std::vector<float> out_vec(dim * count);
    quantizer.DecodeBatch(codes2.data(), out_vec.data(), count);
    for (int64_t i = 0; i < dim * count; ++i) {
        if (vecs[i] * out_vec[i] >= 0) {
            count_same_sign_2++;
        }
    }
    REQUIRE(count_same_sign_2 / (count * dim) > same_sign_rate);
}

template <typename T>
void
TestQuantizerEncodeDecode(
    Quantizer<T>& quant, int64_t dim, int count, float error = 1e-5, bool retrain = true) {
    auto vecs = fixtures::generate_vectors(count, dim, true);
    if (retrain) {
        quant.ReTrain(vecs.data(), count);
    }
    // Test EncodeOne & DecodeOne
    for (uint64_t i = 0; i < count; ++i) {
        std::vector<uint8_t> codes(quant.GetCodeSize());
        quant.EncodeOne(vecs.data() + i * dim, codes.data());
        std::vector<float> out_vec(dim);
        quant.DecodeOne(codes.data(), out_vec.data());
        for (int j = 0; j < dim; ++j) {
            REQUIRE(std::abs(vecs[i * dim + j] - out_vec[j]) < error);
        }
    }

    // Test EncodeBatch & DecodeBatch
    std::vector<uint8_t> codes(quant.GetCodeSize() * count);
    quant.EncodeBatch(vecs.data(), codes.data(), count);
    std::vector<float> out_vec(dim * count);
    quant.DecodeBatch(codes.data(), out_vec.data(), count);
    for (int64_t i = 0; i < dim * count; ++i) {
        REQUIRE(std::abs(vecs[i] - out_vec[i]) < error);
    }
}

template <typename T>
void
TestQuantizerEncodeDecodeSame(Quantizer<T>& quant,
                              int64_t dim,
                              int count,
                              int code_max = 15,
                              float error = 1e-5,
                              bool retrain = true) {
    int seed = 47;
    auto data_uint8 = fixtures::GenerateVectors<uint8_t>(count, dim, seed, 0, 16);
    std::vector<float> data(dim * count);
    for (uint64_t i = 0; i < dim * count; ++i) {
        data[i] = static_cast<float>(data_uint8[i]);
    }
    if (retrain) {
        quant.ReTrain(data.data(), count);
    }

    // Test EncodeOne & DecodeOne
    for (int k = 0; k < count; k++) {
        std::vector<uint8_t> codes(quant.GetCodeSize());
        quant.EncodeOne(data.data() + k * dim, codes.data());
        std::vector<float> out_vec(dim);
        quant.DecodeOne(codes.data(), out_vec.data());
        for (int i = 0; i < dim; ++i) {
            REQUIRE(std::abs(data[k * dim + i] - out_vec[i]) < error);
        }
    }

    // Test EncodeBatch & DecodeBatch
    {
        std::vector<uint8_t> codes(quant.GetCodeSize() * count);
        quant.EncodeBatch(data.data(), codes.data(), count);

        std::vector<float> out_vec(dim * count);
        quant.DecodeBatch(codes.data(), out_vec.data(), count);

        for (int64_t i = 0; i < dim * count; ++i) {
            REQUIRE(std::abs(data[i] - out_vec[i]) < error);
        }
    }
}

template <typename T, MetricType metric>
void
TestComputeCodes(
    Quantizer<T>& quantizer, size_t dim, uint32_t count, float error = 1e-4f, bool retrain = true) {
    auto vecs = fixtures::generate_vectors(count, dim, true);
    if (retrain) {
        quantizer.ReTrain(vecs.data(), count);
    }
    for (int i = 0; i < count; ++i) {
        auto idx1 = random() % count;
        auto idx2 = random() % count;
        std::vector<uint8_t> codes1(quantizer.GetCodeSize());
        std::vector<uint8_t> codes2(quantizer.GetCodeSize());
        quantizer.EncodeOne(vecs.data() + idx1 * dim, codes1.data());
        quantizer.EncodeOne(vecs.data() + idx2 * dim, codes2.data());
        float gt = 0.0;
        float value = quantizer.Compute(codes1.data(), codes2.data());
        if constexpr (metric == vsag::MetricType::METRIC_TYPE_IP ||
                      metric == vsag::MetricType::METRIC_TYPE_COSINE) {
            gt = 1 - InnerProduct(vecs.data() + idx1 * dim, vecs.data() + idx2 * dim, &dim);
        } else if constexpr (metric == vsag::MetricType::METRIC_TYPE_L2SQR) {
            gt = L2Sqr(vecs.data() + idx1 * dim, vecs.data() + idx2 * dim, &dim);
        }
        REQUIRE(std::abs(gt - value) < error);
    }
}

template <typename T, MetricType metric>
void
TestComputeCodesSame(Quantizer<T>& quantizer,
                     size_t dim,
                     uint32_t count,
                     uint32_t code_max = 15,
                     float error = 1e-5f,
                     bool retrain = true) {
    auto data = fixtures::generate_vectors(count, dim, false);
    for (auto& val : data) {
        val = uint8_t(val * code_max);
    }
    if (retrain) {
        quantizer.ReTrain(data.data(), count);
    }
    for (int i = 0; i < count; ++i) {
        auto idx1 = random() % count;
        auto idx2 = random() % count;
        std::vector<uint8_t> codes1(quantizer.GetCodeSize());
        std::vector<uint8_t> codes2(quantizer.GetCodeSize());
        quantizer.EncodeOne(data.data() + idx1 * dim, codes1.data());
        quantizer.EncodeOne(data.data() + idx2 * dim, codes2.data());
        float gt = 0.0f;
        float value = quantizer.Compute(codes1.data(), codes2.data());
        if constexpr (metric == vsag::MetricType::METRIC_TYPE_IP) {
            gt = 1 - InnerProduct(data.data() + idx1 * dim, data.data() + idx2 * dim, &dim);
        } else if constexpr (metric == vsag::MetricType::METRIC_TYPE_L2SQR) {
            gt = L2Sqr(data.data() + idx1 * dim, data.data() + idx2 * dim, &dim);
        } else if constexpr (metric == vsag::MetricType::METRIC_TYPE_COSINE) {
            std::vector<float> v1(dim);
            std::vector<float> v2(dim);
            Normalize(data.data() + idx1 * dim, v1.data(), dim);
            Normalize(data.data() + idx2 * dim, v2.data(), dim);
            gt = 1 - InnerProduct(v1.data(), v2.data(), &dim);
        }
        REQUIRE(std::abs(gt - value) <= error);
    }
}

template <typename T, MetricType metric>
void
TestComputer(Quantizer<T>& quant,
             size_t dim,
             uint32_t count,
             float error = 1e-5f,
             float related_error = 1.0f,
             bool retrain = true,
             float unbounded_numeric_error_rate = 1.0f,
             float unbounded_related_error_rate = 1.0f) {
    auto query_count = 100;
    bool need_normalize = true;
    if constexpr (metric == vsag::MetricType::METRIC_TYPE_COSINE) {
        need_normalize = false;
    }
    auto vecs = fixtures::generate_vectors(count, dim, need_normalize);
    auto queries = fixtures::generate_vectors(query_count, dim, need_normalize, 165);
    if (retrain) {
        quant.ReTrain(vecs.data(), count);
    }

    auto gt_func = [&](int base_idx, int query_idx) -> float {
        if constexpr (metric == vsag::MetricType::METRIC_TYPE_IP) {
            return 1 - InnerProduct(
                           vecs.data() + base_idx * dim, queries.data() + query_idx * dim, &dim);
        } else if constexpr (metric == vsag::MetricType::METRIC_TYPE_L2SQR) {
            return L2Sqr(vecs.data() + base_idx * dim, queries.data() + query_idx * dim, &dim);
        } else if constexpr (metric == vsag::MetricType::METRIC_TYPE_COSINE) {
            std::vector<float> v1(dim);
            std::vector<float> v2(dim);
            Normalize(vecs.data() + base_idx * dim, v1.data(), dim);
            Normalize(queries.data() + query_idx * dim, v2.data(), dim);
            return 1 - InnerProduct(v1.data(), v2.data(), &dim);
        }
    };

    float count_unbounded_related_error = 0, count_unbounded_numeric_error = 0;
    for (int i = 0; i < query_count; ++i) {
        std::shared_ptr<Computer<T>> computer;
        computer = quant.FactoryComputer();
        computer->SetQuery(queries.data() + i * dim);

        // Test Compute One Dist;
        std::vector<uint8_t> codes1(quant.GetCodeSize() * count, 0);
        std::vector<float> dists1(count);
        for (int j = 0; j < count; ++j) {
            auto gt = gt_func(j, i);
            uint8_t* code = codes1.data() + j * quant.GetCodeSize();
            quant.EncodeOne(vecs.data() + j * dim, code);
            quant.ComputeDist(*computer, code, dists1.data() + j);
            REQUIRE(quant.ComputeDist(*computer, code) == dists1[j]);
            if (std::abs(gt - dists1[j]) > error) {
                count_unbounded_numeric_error++;
            }
            if (std::abs(gt - dists1[j]) > related_error * gt) {
                count_unbounded_related_error++;
            }
        }

        // Test Compute Batch
        std::vector<uint8_t> codes2(quant.GetCodeSize() * count);
        std::vector<float> dists2(count);
        quant.EncodeBatch(vecs.data(), codes2.data(), count);
        quant.ComputeBatchDists(*computer, count, codes2.data(), dists2.data());
        for (int j = 0; j < count; ++j) {
            REQUIRE(fixtures::dist_t(dists1[j]) == fixtures::dist_t(dists2[j]));
        }
    }
    REQUIRE(count_unbounded_numeric_error / (query_count * count) <= unbounded_numeric_error_rate);
    REQUIRE(count_unbounded_related_error / (query_count * count) <= unbounded_related_error_rate);
}

template <typename T, MetricType metric, bool uniform = false>
void
TestSerializeAndDeserialize(Quantizer<T>& quant1,
                            Quantizer<T>& quant2,
                            size_t dim,
                            uint32_t count,
                            float error = 1e-5f,
                            float related_error = 1.0f,
                            float unbounded_numeric_error_rate = 1.0f,
                            float unbounded_related_error_rate = 1.0f,
                            bool is_rabitq = false) {
    auto vecs = fixtures::generate_vectors(count, dim);
    quant1.ReTrain(vecs.data(), count);
    fixtures::TempDir dir("quantizer");
    auto filename = dir.GenerateRandomFile();
    std::ofstream outfile(filename.c_str(), std::ios::binary);
    IOStreamWriter writer(outfile);
    quant1.Serialize(writer);
    outfile.close();

    std::ifstream infile(filename.c_str(), std::ios::binary);
    IOStreamReader reader(infile);
    quant2.Deserialize(reader);
    infile.close();

    REQUIRE(quant1.GetCodeSize() == quant2.GetCodeSize());
    REQUIRE(quant1.GetDim() == quant2.GetDim());

    if (not is_rabitq) {
        TestQuantizerEncodeDecode<T>(quant2, dim, count, error, false);
        if constexpr (uniform == false) {
            TestComputer<T, metric>(quant2,
                                    dim,
                                    count,
                                    error,
                                    related_error,
                                    false,
                                    unbounded_numeric_error_rate,
                                    unbounded_related_error_rate);
            TestComputeCodes<T, metric>(quant2, dim, count, error, false);
        } else {
            TestComputeCodesSame<T, metric>(quant2, dim, count, error, false);
        }
    } else {
        TestComputer<T, metric>(quant2,
                                dim,
                                count,
                                error,
                                related_error,
                                true,
                                unbounded_numeric_error_rate,
                                unbounded_related_error_rate);
        TestEncodeDecodeRaBitQ<T>(quant2, dim, count);
        REQUIRE_THROWS(TestComputeCodes<T, metric>(quant2, dim, count, error, false));
    }
}
