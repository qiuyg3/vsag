
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

#if defined(ENABLE_SSE)
#include <x86intrin.h>
#endif

#include <cmath>

#include "simd.h"

#define PORTABLE_ALIGN32 __attribute__((aligned(32)))
#define PORTABLE_ALIGN64 __attribute__((aligned(64)))

namespace vsag::sse {

float
L2Sqr(const void* pVect1v, const void* pVect2v, const void* qty_ptr) {
    auto* pVect1 = (float*)pVect1v;
    auto* pVect2 = (float*)pVect2v;
    auto qty = *((size_t*)qty_ptr);
    return sse::FP32ComputeL2Sqr(pVect1, pVect2, qty);
}

float
InnerProduct(const void* pVect1v, const void* pVect2v, const void* qty_ptr) {
    auto* pVect1 = (float*)pVect1v;
    auto* pVect2 = (float*)pVect2v;
    auto qty = *((size_t*)qty_ptr);
    return sse::FP32ComputeIP(pVect1, pVect2, qty);
}

float
InnerProductDistance(const void* pVect1, const void* pVect2, const void* qty_ptr) {
    return 1.0f - sse::InnerProduct(pVect1, pVect2, qty_ptr);
}

float
INT8InnerProduct(const void* pVect1v, const void* pVect2v, const void* qty_ptr) {
    return generic::INT8InnerProduct(pVect1v, pVect2v, qty_ptr);  // TODO(LHT): implement
}

float
INT8InnerProductDistance(const void* pVect1v, const void* pVect2v, const void* qty_ptr) {
    return -sse::INT8InnerProduct(pVect1v, pVect2v, qty_ptr);
}

void
PQDistanceFloat256(const void* single_dim_centers, float single_dim_val, void* result) {
#if defined(ENABLE_SSE)
    const auto* float_centers = (const float*)single_dim_centers;
    auto* float_result = (float*)result;
    for (size_t idx = 0; idx < 256; idx += 4) {
        __m128 v_centers_dim = _mm_loadu_ps(float_centers + idx);
        __m128 v_query_vec = _mm_set1_ps(single_dim_val);
        __m128 v_diff = _mm_sub_ps(v_centers_dim, v_query_vec);
        __m128 v_diff_sq = _mm_mul_ps(v_diff, v_diff);
        __m128 v_chunk_dists = _mm_loadu_ps(&float_result[idx]);
        v_chunk_dists = _mm_add_ps(v_chunk_dists, v_diff_sq);
        _mm_storeu_ps(&float_result[idx], v_chunk_dists);
    }
#else
    return generic::PQDistanceFloat256(single_dim_centers, single_dim_val, result);
#endif
}

#if defined(ENABLE_SSE)
__inline __m128i __attribute__((__always_inline__)) load_4_char(const uint8_t* data) {
    return _mm_set_epi8(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, data[3], data[2], data[1], data[0]);
}
#endif

#if defined(ENABLE_SSE)
__inline __m128i __attribute__((__always_inline__)) load_4_short(const uint16_t* data) {
    return _mm_set_epi16(data[3], 0, data[2], 0, data[1], 0, data[0], 0);
}
#endif

float
FP32ComputeIP(const float* query, const float* codes, uint64_t dim) {
#if defined(ENABLE_SSE)
    const int n = dim / 4;
    if (n == 0) {
        return generic::FP32ComputeIP(query, codes, dim);
    }
    // process 4 floats at a time
    __m128 sum = _mm_setzero_ps();  // initialize to 0
    for (int i = 0; i < n; ++i) {
        __m128 a = _mm_loadu_ps(query + i * 4);   // load 4 floats from memory
        __m128 b = _mm_loadu_ps(codes + i * 4);   // load 4 floats from memory
        sum = _mm_add_ps(sum, _mm_mul_ps(a, b));  // accumulate the product
    }
    alignas(16) float result[4];
    _mm_store_ps(result, sum);  // store the accumulated result into an array
    float ip = result[0] + result[1] + result[2] +
               result[3];  // calculate the sum of the accumulated results
    ip += generic::FP32ComputeIP(query + n * 4, codes + n * 4, dim - n * 4);
    return ip;
#else
    return vsag::generic::FP32ComputeIP(query, codes, dim);
#endif
}

float
FP32ComputeL2Sqr(const float* query, const float* codes, uint64_t dim) {
#if defined(ENABLE_SSE)
    const uint64_t n = dim / 4;
    if (n == 0) {
        return generic::FP32ComputeL2Sqr(query, codes, dim);
    }
    __m128 sum = _mm_setzero_ps();
    for (int i = 0; i < n; ++i) {
        __m128 a = _mm_loadu_ps(query + i * 4);
        __m128 b = _mm_loadu_ps(codes + i * 4);
        __m128 diff = _mm_sub_ps(a, b);
        sum = _mm_add_ps(sum, _mm_mul_ps(diff, diff));
    }
    alignas(16) float result[4];
    _mm_store_ps(result, sum);
    float l2 = result[0] + result[1] + result[2] + result[3];
    l2 += generic::FP32ComputeL2Sqr(query + n * 4, codes + n * 4, dim - n * 4);
    return l2;
#else
    return vsag::generic::FP32ComputeL2Sqr(query, codes, dim);
#endif
}

float
BF16ComputeIP(const uint8_t* query, const uint8_t* codes, uint64_t dim) {
#if defined(ENABLE_SSE)
    // Initialize the sum to 0
    __m128 sum = _mm_setzero_ps();
    const auto* query_bf16 = (const uint16_t*)(query);
    const auto* codes_bf16 = (const uint16_t*)(codes);
    // Process the data in 128-bit chunks
    uint64_t i = 0;
    for (; i + 3 < dim; i += 4) {
        // Load data into registers
        __m128i query_vec = load_4_short(query_bf16 + i);
        __m128 query_float = _mm_castsi128_ps(query_vec);

        // Load data into registers
        __m128i code_vec = load_4_short(codes_bf16 + i);
        __m128 code_float = _mm_castsi128_ps(code_vec);

        __m128 mul = _mm_mul_ps(query_float, code_float);
        sum = _mm_add_ps(sum, mul);
    }

    // Horizontal addition
    sum = _mm_hadd_ps(sum, sum);
    sum = _mm_hadd_ps(sum, sum);

    // Extract the result from the register
    alignas(16) float result[4];
    _mm_store_ps(result, sum);

    return result[0] + generic::BF16ComputeIP(query + i * 2, codes + i * 2, dim - i);
#else
    return generic::BF16ComputeIP(query, codes, dim);
#endif
}

float
BF16ComputeL2Sqr(const uint8_t* query, const uint8_t* codes, uint64_t dim) {
#if defined(ENABLE_SSE)
    // Initialize the sum to 0
    __m128 sum = _mm_setzero_ps();
    const auto* query_bf16 = (const uint16_t*)(query);
    const auto* codes_bf16 = (const uint16_t*)(codes);

    // Process the data in 128-bit chunks
    uint64_t i = 0;
    for (; i + 3 < dim; i += 4) {
        // Load data into registers
        __m128i query_vec = load_4_short(query_bf16 + i);
        __m128 query_float = _mm_castsi128_ps(query_vec);

        // Load data into registers
        __m128i code_vec = load_4_short(codes_bf16 + i);
        __m128 code_float = _mm_castsi128_ps(code_vec);

        __m128 diff = _mm_sub_ps(code_float, query_float);
        sum = _mm_add_ps(sum, _mm_mul_ps(diff, diff));
    }

    // Horizontal addition
    sum = _mm_hadd_ps(sum, sum);
    sum = _mm_hadd_ps(sum, sum);

    // Extract the result from the register
    alignas(16) float result[4];
    _mm_store_ps(result, sum);

    return result[0] + generic::BF16ComputeL2Sqr(query + i * 2, codes + i * 2, dim - i);
#else
    return generic::BF16ComputeL2Sqr(query, codes, dim);
#endif
}

float
FP16ComputeIP(const uint8_t* query, const uint8_t* codes, uint64_t dim) {
    return generic::FP16ComputeIP(query, codes, dim);
}

float
FP16ComputeL2Sqr(const uint8_t* query, const uint8_t* codes, uint64_t dim) {
    return generic::FP16ComputeL2Sqr(query, codes, dim);
}

float
SQ8ComputeIP(const float* query,
             const uint8_t* codes,
             const float* lower_bound,
             const float* diff,
             uint64_t dim) {
#if defined(ENABLE_SSE)
    // Initialize the sum to 0
    __m128 sum = _mm_setzero_ps();

    // Process the data in 128-bit chunks
    uint64_t i = 0;
    for (; i + 3 < dim; i += 4) {
        // Load data into registers
        __m128i code_values = load_4_char(codes + i);
        __m128 code_floats = _mm_cvtepi32_ps(_mm_cvtepu8_epi32(code_values));
        __m128 query_values = _mm_loadu_ps(query + i);
        __m128 diff_values = _mm_loadu_ps(diff + i);
        __m128 lower_bound_values = _mm_loadu_ps(lower_bound + i);

        // Perform calculations
        __m128 scaled_codes = _mm_mul_ps(_mm_div_ps(code_floats, _mm_set1_ps(255.0f)), diff_values);
        __m128 adjusted_codes = _mm_add_ps(scaled_codes, lower_bound_values);
        __m128 val = _mm_mul_ps(query_values, adjusted_codes);
        sum = _mm_add_ps(sum, val);
    }

    // Horizontal addition
    sum = _mm_hadd_ps(sum, sum);
    sum = _mm_hadd_ps(sum, sum);

    // Extract the result from the register
    alignas(16) float result[4];
    _mm_store_ps(result, sum);

    return result[0] +
           generic::SQ8ComputeIP(query + i, codes + i, lower_bound + i, diff + i, dim - i);
#else
    return generic::SQ8ComputeIP(query, codes, lower_bound, diff, dim);
#endif
}

float
SQ8ComputeL2Sqr(const float* query,
                const uint8_t* codes,
                const float* lower_bound,
                const float* diff,
                uint64_t dim) {
#if defined(ENABLE_SSE)
    __m128 sum = _mm_setzero_ps();

    // Process the data in 128-bit chunks
    uint64_t i = 0;
    for (; i + 3 < dim; i += 4) {
        // Load data into registers
        __m128i code_values = _mm_cvtepu8_epi32(load_4_char(codes + i));
        __m128 code_floats = _mm_div_ps(_mm_cvtepi32_ps(code_values), _mm_set1_ps(255.0f));
        __m128 diff_values = _mm_loadu_ps(diff + i);
        __m128 lower_bound_values = _mm_loadu_ps(lower_bound + i);
        __m128 query_values = _mm_loadu_ps(query + i);

        // Perform calculations
        __m128 scaled_codes = _mm_mul_ps(code_floats, diff_values);
        scaled_codes = _mm_add_ps(scaled_codes, lower_bound_values);
        __m128 val = _mm_sub_ps(query_values, scaled_codes);
        val = _mm_mul_ps(val, val);
        sum = _mm_add_ps(sum, val);
    }
    // Perform horizontal addition
    sum = _mm_hadd_ps(sum, sum);
    sum = _mm_hadd_ps(sum, sum);

    // Extract the result from the register
    float result;
    _mm_store_ss(&result, sum);

    result += generic::SQ8ComputeL2Sqr(query + i, codes + i, lower_bound + i, diff + i, dim - i);

    return result;
#else
    return generic::SQ8ComputeL2Sqr(query, codes, lower_bound, diff, dim);
#endif
}

float
SQ8ComputeCodesIP(const uint8_t* codes1,
                  const uint8_t* codes2,
                  const float* lower_bound,
                  const float* diff,
                  uint64_t dim) {
#if defined(ENABLE_SSE)
    __m128 sum = _mm_setzero_ps();
    uint64_t i = 0;
    for (; i + 3 < dim; i += 4) {
        // Load data into registers
        __m128i code1_values = load_4_char(codes1 + i);
        __m128i code2_values = load_4_char(codes2 + i);
        __m128i codes1_128 = _mm_cvtepu8_epi32(code1_values);
        __m128i codes2_128 = _mm_cvtepu8_epi32(code2_values);
        __m128 codes1_floats = _mm_div_ps(_mm_cvtepi32_ps(codes1_128), _mm_set1_ps(255.0f));
        __m128 codes2_floats = _mm_div_ps(_mm_cvtepi32_ps(codes2_128), _mm_set1_ps(255.0f));
        __m128 diff_values = _mm_loadu_ps(diff + i);
        __m128 lower_bound_values = _mm_loadu_ps(lower_bound + i);
        // Perform calculations
        __m128 scaled_codes1 =
            _mm_add_ps(_mm_mul_ps(codes1_floats, diff_values), lower_bound_values);
        __m128 scaled_codes2 =
            _mm_add_ps(_mm_mul_ps(codes2_floats, diff_values), lower_bound_values);
        __m128 val = _mm_mul_ps(scaled_codes1, scaled_codes2);
        sum = _mm_add_ps(sum, val);
    }
    // Horizontal addition
    sum = _mm_hadd_ps(sum, sum);
    sum = _mm_hadd_ps(sum, sum);
    // Extract the result from the register
    float result;
    _mm_store_ss(&result, sum);
    result +=
        generic::SQ8ComputeCodesIP(codes1 + i, codes2 + i, lower_bound + i, diff + i, dim - i);
    return result;
#else
    return generic::SQ8ComputeCodesIP(codes1, codes2, lower_bound, diff, dim);
#endif
}

float
SQ8ComputeCodesL2Sqr(const uint8_t* codes1,
                     const uint8_t* codes2,
                     const float* lower_bound,
                     const float* diff,
                     uint64_t dim) {
#if defined(ENABLE_SSE)
    __m128 sum = _mm_setzero_ps();
    uint64_t i = 0;
    for (; i + 3 < dim; i += 4) {
        // Load data into registers
        __m128i code1_values = load_4_char(codes1 + i);
        __m128i code2_values = load_4_char(codes2 + i);
        __m128i codes1_128 = _mm_cvtepu8_epi32(code1_values);
        __m128i codes2_128 = _mm_cvtepu8_epi32(code2_values);
        __m128 codes1_floats = _mm_div_ps(_mm_cvtepi32_ps(codes1_128), _mm_set1_ps(255.0f));
        __m128 codes2_floats = _mm_div_ps(_mm_cvtepi32_ps(codes2_128), _mm_set1_ps(255.0f));
        __m128 diff_values = _mm_loadu_ps(diff + i);
        __m128 lower_bound_values = _mm_loadu_ps(lower_bound + i);
        // Perform calculations
        __m128 scaled_codes1 =
            _mm_add_ps(_mm_mul_ps(codes1_floats, diff_values), lower_bound_values);
        __m128 scaled_codes2 =
            _mm_add_ps(_mm_mul_ps(codes2_floats, diff_values), lower_bound_values);
        __m128 val = _mm_sub_ps(scaled_codes1, scaled_codes2);
        val = _mm_mul_ps(val, val);
        sum = _mm_add_ps(sum, val);
    }
    // Horizontal addition
    sum = _mm_hadd_ps(sum, sum);
    sum = _mm_hadd_ps(sum, sum);
    // Extract the result from the register
    float result;
    _mm_store_ss(&result, sum);
    result +=
        generic::SQ8ComputeCodesL2Sqr(codes1 + i, codes2 + i, lower_bound + i, diff + i, dim - i);
    return result;
#else
    return generic::SQ8ComputeCodesIP(codes1, codes2, lower_bound, diff, dim);
#endif
}

float
SQ4ComputeIP(const float* query,
             const uint8_t* codes,
             const float* lower_bound,
             const float* diff,
             uint64_t dim) {
    return generic::SQ4ComputeIP(query, codes, lower_bound, diff, dim);
}

float
SQ4ComputeL2Sqr(const float* query,
                const uint8_t* codes,
                const float* lower_bound,
                const float* diff,
                uint64_t dim) {
    return generic::SQ4ComputeL2Sqr(query, codes, lower_bound, diff, dim);
}

float
SQ4ComputeCodesIP(const uint8_t* codes1,
                  const uint8_t* codes2,
                  const float* lower_bound,
                  const float* diff,
                  uint64_t dim) {
    return generic::SQ4ComputeCodesIP(codes1, codes2, lower_bound, diff, dim);
}

float
SQ4ComputeCodesL2Sqr(const uint8_t* codes1,
                     const uint8_t* codes2,
                     const float* lower_bound,
                     const float* diff,
                     uint64_t dim) {
    return generic::SQ4ComputeCodesL2Sqr(codes1, codes2, lower_bound, diff, dim);
}

float
SQ4UniformComputeCodesIP(const uint8_t* codes1, const uint8_t* codes2, uint64_t dim) {
#if defined(ENABLE_SSE)
    if (dim == 0) {
        return 0;
    }
    alignas(128) int16_t temp[8];
    int32_t result = 0;
    uint64_t d = 0;
    __m128i sum = _mm_setzero_si128();
    __m128i mask = _mm_set1_epi8(0xf);
    for (; d + 31 < dim; d += 32) {
        auto xx = _mm_loadu_si128((__m128i*)(codes1 + (d >> 1)));
        auto yy = _mm_loadu_si128((__m128i*)(codes2 + (d >> 1)));
        auto xx1 = _mm_and_si128(xx, mask);                     // 16 * 8bits
        auto xx2 = _mm_and_si128(_mm_srli_epi16(xx, 4), mask);  // 16 * 8bits
        auto yy1 = _mm_and_si128(yy, mask);
        auto yy2 = _mm_and_si128(_mm_srli_epi16(yy, 4), mask);

        sum = _mm_add_epi16(sum, _mm_maddubs_epi16(xx1, yy1));
        sum = _mm_add_epi16(sum, _mm_maddubs_epi16(xx2, yy2));
    }
    _mm_store_si128((__m128i*)temp, sum);
    for (int i = 0; i < 8; ++i) {
        result += temp[i];
    }
    result += generic::SQ4UniformComputeCodesIP(codes1 + (d >> 1), codes2 + (d >> 1), dim - d);
    return result;
#else
    return generic::SQ4UniformComputeCodesIP(codes1, codes2, dim);
#endif
}

float
SQ8UniformComputeCodesIP(const uint8_t* codes1, const uint8_t* codes2, uint64_t dim) {
#if defined(ENABLE_SSE)
    if (dim == 0) {
        return 0;
    }
    alignas(128) int32_t temp[4];
    int32_t result = 0;
    uint64_t d = 0;
    __m128i sum = _mm_setzero_si128();
    __m128i mask = _mm_set1_epi16(0xff);
    for (; d + 15 < dim; d += 16) {
        auto xx = _mm_loadu_si128((__m128i*)(codes1 + d));
        auto yy = _mm_loadu_si128((__m128i*)(codes2 + d));

        auto xx1 = _mm_and_si128(xx, mask);  // 16 * 8bits
        auto xx2 = _mm_srli_epi16(xx, 8);    // 16 * 8bits
        auto yy1 = _mm_and_si128(yy, mask);
        auto yy2 = _mm_srli_epi16(yy, 8);

        sum = _mm_add_epi32(sum, _mm_madd_epi16(xx1, yy1));
        sum = _mm_add_epi32(sum, _mm_madd_epi16(xx2, yy2));
    }
    _mm_store_si128((__m128i*)temp, sum);
    for (int i = 0; i < 4; ++i) {
        result += temp[i];
    }
    result +=
        static_cast<int32_t>(generic::SQ8UniformComputeCodesIP(codes1 + d, codes2 + d, dim - d));
    return static_cast<float>(result);
#else
    return generic::SQ8UniformComputeCodesIP(codes1, codes2, dim);
#endif
}

float
RaBitQFloatBinaryIP(const float* vector, const uint8_t* bits, uint64_t dim, float inv_sqrt_d) {
#if defined(ENABLE_SSE)
    return generic::RaBitQFloatBinaryIP(vector, bits, dim, inv_sqrt_d);  // TODO(zxy): implement
#else
    return generic::RaBitQFloatBinaryIP(vector, bits, dim, inv_sqrt_d);
#endif
}

void
DivScalar(const float* from, float* to, uint64_t dim, float scalar) {
#if defined(ENABLE_SSE)
    if (dim == 0) {
        return;
    }
    if (scalar == 0) {
        scalar = 1.0f;  // TODO(LHT): logger?
    }
    int i = 0;
    __m128 scalarVec = _mm_set1_ps(scalar);
    for (; i + 3 < dim; i += 4) {
        __m128 vec = _mm_loadu_ps(from + i);
        vec = _mm_div_ps(vec, scalarVec);
        _mm_storeu_ps(to + i, vec);
    }
    generic::DivScalar(from + i, to + i, dim - i, scalar);
#else
    generic::DivScalar(from, to, dim, scalar);
#endif
}

float
Normalize(const float* from, float* to, uint64_t dim) {
    float norm = std::sqrt(FP32ComputeIP(from, from, dim));
    sse::DivScalar(from, to, dim, norm);
    return norm;
}

void
Prefetch(const void* data) {
#if defined(ENABLE_SSE)
    _mm_prefetch(data, _MM_HINT_T0);
#endif
};

}  // namespace vsag::sse
