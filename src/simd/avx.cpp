
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

#if defined(ENABLE_AVX)
#include <immintrin.h>
#endif

#include <cmath>
#include <cstdint>

#include "simd.h"

#define PORTABLE_ALIGN32 __attribute__((aligned(32)))
#define PORTABLE_ALIGN64 __attribute__((aligned(64)))

namespace vsag::avx {

float
L2Sqr(const void* pVect1v, const void* pVect2v, const void* qty_ptr) {
    auto* pVect1 = (float*)pVect1v;
    auto* pVect2 = (float*)pVect2v;
    auto qty = *((size_t*)qty_ptr);
    return avx::FP32ComputeL2Sqr(pVect1, pVect2, qty);
}

float
InnerProduct(const void* pVect1v, const void* pVect2v, const void* qty_ptr) {
    auto* pVect1 = (float*)pVect1v;
    auto* pVect2 = (float*)pVect2v;
    auto qty = *((size_t*)qty_ptr);
    return avx::FP32ComputeIP(pVect1, pVect2, qty);
}

float
InnerProductDistance(const void* pVect1, const void* pVect2, const void* qty_ptr) {
    return 1.0f - avx::InnerProduct(pVect1, pVect2, qty_ptr);
}

float
INT8InnerProduct(const void* pVect1v, const void* pVect2v, const void* qty_ptr) {
    return sse::INT8InnerProduct(pVect1v, pVect2v, qty_ptr);  // TODO(LHT): implement
}

float
INT8InnerProductDistance(const void* pVect1v, const void* pVect2v, const void* qty_ptr) {
    return -avx::INT8InnerProduct(pVect1v, pVect2v, qty_ptr);
}

void
PQDistanceFloat256(const void* single_dim_centers, float single_dim_val, void* result) {
#if defined(ENABLE_AVX)
    auto* float_centers = (const float*)single_dim_centers;
    auto* float_result = (float*)result;
    for (size_t idx = 0; idx < 256; idx += 8) {
        __m256 v_centers_dim = _mm256_loadu_ps(float_centers + idx);
        __m256 v_query_vec = _mm256_set1_ps(single_dim_val);
        __m256 v_diff = _mm256_sub_ps(v_centers_dim, v_query_vec);
        __m256 v_diff_sq = _mm256_mul_ps(v_diff, v_diff);
        __m256 v_chunk_dists = _mm256_loadu_ps(&float_result[idx]);
        v_chunk_dists = _mm256_add_ps(v_chunk_dists, v_diff_sq);
        _mm256_storeu_ps(&float_result[idx], v_chunk_dists);
    }
#else
    return sse::PQDistanceFloat256(single_dim_centers, single_dim_val, result);
#endif
}

#if defined(ENABLE_AVX)
__inline __m256i __attribute__((__always_inline__)) load_8_char_and_convert(const uint8_t* data) {
    __m128i first_8 =
        _mm_set_epi8(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, data[3], data[2], data[1], data[0]);
    __m128i second_8 =
        _mm_set_epi8(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, data[7], data[6], data[5], data[4]);
    __m128i first_32 = _mm_cvtepu8_epi32(first_8);
    __m128i second_32 = _mm_cvtepu8_epi32(second_8);
    return _mm256_set_m128i(second_32, first_32);
}
#endif

float
FP32ComputeIP(const float* query, const float* codes, uint64_t dim) {
#if defined(ENABLE_AVX)
    const uint32_t n = dim / 8;
    if (n == 0) {
        return sse::FP32ComputeIP(query, codes, dim);
    }
    // process 8 floats at a time
    __m256 sum = _mm256_setzero_ps();  // initialize to 0
    for (int i = 0; i < n; ++i) {
        __m256 a = _mm256_loadu_ps(query + i * 8);      // load 8 floats from memory
        __m256 b = _mm256_loadu_ps(codes + i * 8);      // load 8 floats from memory
        sum = _mm256_add_ps(sum, _mm256_mul_ps(a, b));  // accumulate the product
    }
    alignas(32) float result[8];
    _mm256_store_ps(result, sum);  // store the accumulated result into an array
    float ip = result[0] + result[1] + result[2] + result[3] + result[4] + result[5] + result[6] +
               result[7];  // calculate the sum of the accumulated results
    ip += sse::FP32ComputeIP(query + n * 8, codes + n * 8, dim - n * 8);
    return ip;
#else
    return sse::FP32ComputeIP(query, codes, dim);
#endif
}

float
FP32ComputeL2Sqr(const float* query, const float* codes, uint64_t dim) {
#if defined(ENABLE_AVX)
    const int n = dim / 8;
    if (n == 0) {
        return sse::FP32ComputeL2Sqr(query, codes, dim);
    }
    // process 8 floats at a time
    __m256 sum = _mm256_setzero_ps();  // initialize to 0
    for (int i = 0; i < n; ++i) {
        __m256 a = _mm256_loadu_ps(query + i * 8);            // load 8 floats from memory
        __m256 b = _mm256_loadu_ps(codes + i * 8);            // load 8 floats from memory
        __m256 diff = _mm256_sub_ps(a, b);                    // calculate the difference
        sum = _mm256_add_ps(sum, _mm256_mul_ps(diff, diff));  // accumulate the squared difference
    }
    alignas(32) float result[8];
    _mm256_store_ps(result, sum);  // store the accumulated result into an array
    float l2 = result[0] + result[1] + result[2] + result[3] + result[4] + result[5] + result[6] +
               result[7];  // calculate the sum of the accumulated results
    l2 += sse::FP32ComputeL2Sqr(query + n * 8, codes + n * 8, dim - n * 8);
    return l2;
#else
    return sse::FP32ComputeL2Sqr(query, codes, dim);
#endif
}

#if defined(ENABLE_AVX)
__inline __m256i __attribute__((__always_inline__)) load_8_short(const uint16_t* data) {
    return _mm256_set_epi16(data[7],
                            0,
                            data[6],
                            0,
                            data[5],
                            0,
                            data[4],
                            0,
                            data[3],
                            0,
                            data[2],
                            0,
                            data[1],
                            0,
                            data[0],
                            0);
}
#endif

float
BF16ComputeIP(const uint8_t* query, const uint8_t* codes, uint64_t dim) {
#if defined(ENABLE_AVX)
    // Initialize the sum to 0
    __m256 sum = _mm256_setzero_ps();
    const auto* query_bf16 = (const uint16_t*)(query);
    const auto* codes_bf16 = (const uint16_t*)(codes);

    // Process the data in 128-bit chunks
    uint64_t i = 0;
    for (; i + 7 < dim; i += 8) {
        // Load data into registers
        __m256i query_shift = load_8_short(query_bf16 + i);
        __m256 query_float = _mm256_castsi256_ps(query_shift);

        // Load data into registers
        __m256i code_shift = load_8_short(codes_bf16 + i);
        __m256 code_float = _mm256_castsi256_ps(code_shift);

        __m256 val = _mm256_mul_ps(code_float, query_float);
        sum = _mm256_add_ps(sum, val);
    }

    alignas(32) float result[8];
    _mm256_store_ps(result, sum);  // store the accumulated result into an array
    float ip = result[0] + result[1] + result[2] + result[3] + result[4] + result[5] + result[6] +
               result[7];  // calculate the sum of the accumulated results

    return ip + sse::BF16ComputeIP(query + i * 2, codes + i * 2, dim - i);
#else
    return sse::BF16ComputeIP(query, codes, dim);
#endif
}

float
BF16ComputeL2Sqr(const uint8_t* query, const uint8_t* codes, uint64_t dim) {
#if defined(ENABLE_AVX)
    // Initialize the sum to 0
    __m256 sum = _mm256_setzero_ps();
    const auto* query_bf16 = (const uint16_t*)(query);
    const auto* codes_bf16 = (const uint16_t*)(codes);

    // Process the data in 128-bit chunks
    uint64_t i = 0;
    for (; i + 7 < dim; i += 8) {
        // Load data into registers
        __m256i query_shift = load_8_short(query_bf16 + i);
        __m256 query_float = _mm256_castsi256_ps(query_shift);

        // Load data into registers
        __m256i code_shift = load_8_short(codes_bf16 + i);
        __m256 code_float = _mm256_castsi256_ps(code_shift);

        __m256 diff = _mm256_sub_ps(code_float, query_float);
        __m256 val = _mm256_mul_ps(diff, diff);
        sum = _mm256_add_ps(sum, val);
    }

    alignas(32) float result[8];
    _mm256_store_ps(result, sum);  // store the accumulated result into an array
    float l2 = result[0] + result[1] + result[2] + result[3] + result[4] + result[5] + result[6] +
               result[7];  // calculate the sum of the accumulated results

    return l2 + sse::BF16ComputeL2Sqr(query + i * 2, codes + i * 2, dim - i);
#else
    return sse::BF16ComputeL2Sqr(query, codes, dim);
#endif
}

float
FP16ComputeIP(const uint8_t* query, const uint8_t* codes, uint64_t dim) {
#if defined(ENABLE_AVX)
    // Initialize the sum to 0
    __m256 sum = _mm256_setzero_ps();
    const auto* query_fp16 = (const uint16_t*)(query);
    const auto* codes_fp16 = (const uint16_t*)(codes);

    // Process the data in 128-bit chunks
    uint64_t i = 0;
    for (; i + 7 < dim; i += 8) {
        // Load data into registers
        __m128i query_load = _mm_loadu_si128(reinterpret_cast<const __m128i*>(query_fp16 + i));
        __m256 query_float = _mm256_cvtph_ps(query_load);

        // Load data into registers
        __m128i code_load = _mm_loadu_si128(reinterpret_cast<const __m128i*>(codes_fp16 + i));
        __m256 code_float = _mm256_cvtph_ps(code_load);

        __m256 val = _mm256_mul_ps(code_float, query_float);
        sum = _mm256_add_ps(sum, val);
    }

    alignas(32) float result[8];
    _mm256_store_ps(result, sum);  // store the accumulated result into an array
    float ip = result[0] + result[1] + result[2] + result[3] + result[4] + result[5] + result[6] +
               result[7];  // calculate the sum of the accumulated results

    return ip + sse::FP16ComputeIP(query + i * 2, codes + i * 2, dim - i);
#else
    return sse::FP16ComputeIP(query, codes, dim);
#endif
}

float
FP16ComputeL2Sqr(const uint8_t* query, const uint8_t* codes, uint64_t dim) {
#if defined(ENABLE_AVX)
    // Initialize the sum to 0
    __m256 sum = _mm256_setzero_ps();
    const auto* query_fp16 = (const uint16_t*)(query);
    const auto* codes_fp16 = (const uint16_t*)(codes);

    // Process the data in 128-bit chunks
    uint64_t i = 0;
    for (; i + 7 < dim; i += 8) {
        // Load data into registers
        __m128i query_load = _mm_loadu_si128(reinterpret_cast<const __m128i*>(query_fp16 + i));
        __m256 query_float = _mm256_cvtph_ps(query_load);

        // Load data into registers
        __m128i code_load = _mm_loadu_si128(reinterpret_cast<const __m128i*>(codes_fp16 + i));
        __m256 code_float = _mm256_cvtph_ps(code_load);

        __m256 diff = _mm256_sub_ps(code_float, query_float);
        __m256 val = _mm256_mul_ps(diff, diff);
        sum = _mm256_add_ps(sum, val);
    }

    alignas(32) float result[8];
    _mm256_store_ps(result, sum);  // store the accumulated result into an array
    float l2 = result[0] + result[1] + result[2] + result[3] + result[4] + result[5] + result[6] +
               result[7];  // calculate the sum of the accumulated results

    return l2 + sse::FP16ComputeL2Sqr(query + i * 2, codes + i * 2, dim - i);
#else
    return sse::FP16ComputeL2Sqr(query, codes, dim);
#endif
}

float
SQ8ComputeIP(const float* query,
             const uint8_t* codes,
             const float* lower_bound,
             const float* diff,
             uint64_t dim) {
#if defined(ENABLE_AVX)
    __m256 sum = _mm256_setzero_ps();
    uint64_t i = 0;

    for (; i + 7 < dim; i += 8) {
        __m256i code_values = load_8_char_and_convert(codes + i);
        __m256 code_floats = _mm256_cvtepi32_ps(code_values);
        __m256 query_values = _mm256_loadu_ps(query + i);
        __m256 diff_values = _mm256_loadu_ps(diff + i);
        __m256 lower_bound_values = _mm256_loadu_ps(lower_bound + i);

        __m256 scaled_codes =
            _mm256_mul_ps(_mm256_div_ps(code_floats, _mm256_set1_ps(255.0f)), diff_values);
        __m256 adjusted_codes = _mm256_add_ps(scaled_codes, lower_bound_values);
        __m256 val = _mm256_mul_ps(query_values, adjusted_codes);
        sum = _mm256_add_ps(sum, val);
    }

    __m128 sum_high = _mm256_extractf128_ps(sum, 1);
    __m128 sum_low = _mm256_castps256_ps128(sum);
    __m128 sum_final = _mm_add_ps(sum_low, sum_high);

    alignas(16) float result[4];
    _mm_store_ps(result, sum_final);
    float finalResult = result[0] + result[1] + result[2] + result[3];

    // Process the remaining elements recursively
    finalResult += sse::SQ8ComputeIP(query + i, codes + i, lower_bound + i, diff + i, dim - i);
    return finalResult;
#else
    return sse::SQ8ComputeIP(query, codes, lower_bound, diff, dim);
#endif
}

float
SQ8ComputeL2Sqr(const float* query,
                const uint8_t* codes,
                const float* lower_bound,
                const float* diff,
                uint64_t dim) {
#if defined(ENABLE_AVX)
    __m256 sum = _mm256_setzero_ps();
    uint64_t i = 0;

    for (; i + 7 < dim; i += 8) {
        // Load data into registers
        __m256i code_values = load_8_char_and_convert(codes + i);
        __m256 code_floats = _mm256_div_ps(_mm256_cvtepi32_ps(code_values), _mm256_set1_ps(255.0f));
        __m256 diff_values = _mm256_loadu_ps(diff + i);
        __m256 lower_bound_values = _mm256_loadu_ps(lower_bound + i);
        __m256 query_values = _mm256_loadu_ps(query + i);

        // Perform calculations
        __m256 scaled_codes = _mm256_mul_ps(code_floats, diff_values);
        scaled_codes = _mm256_add_ps(scaled_codes, lower_bound_values);
        __m256 val = _mm256_sub_ps(query_values, scaled_codes);
        val = _mm256_mul_ps(val, val);
        sum = _mm256_add_ps(sum, val);
    }

    // Horizontal addition
    __m128 sum_high = _mm256_extractf128_ps(sum, 1);
    __m128 sum_low = _mm256_castps256_ps128(sum);
    __m128 sum_final = _mm_add_ps(sum_low, sum_high);
    sum_final = _mm_hadd_ps(sum_final, sum_final);
    sum_final = _mm_hadd_ps(sum_final, sum_final);

    // Extract the result from the register
    float result;
    _mm_store_ss(&result, sum_final);

    // Process the remaining elements
    result += sse::SQ8ComputeL2Sqr(query + i, codes + i, lower_bound + i, diff + i, dim - i);
    return result;
#else
    return vsag::sse::SQ8ComputeL2Sqr(query, codes, lower_bound, diff, dim);  // TODO
#endif
}

float
SQ8ComputeCodesIP(const uint8_t* codes1,
                  const uint8_t* codes2,
                  const float* lower_bound,
                  const float* diff,
                  uint64_t dim) {
#if defined(ENABLE_AVX)
    __m256 sum = _mm256_setzero_ps();
    uint64_t i = 0;
    for (; i + 7 < dim; i += 8) {
        // Load data into registers
        __m256i codes1_256 = load_8_char_and_convert(codes1 + i);
        __m256i codes2_256 = load_8_char_and_convert(codes2 + i);
        __m256 code1_floats = _mm256_div_ps(_mm256_cvtepi32_ps(codes1_256), _mm256_set1_ps(255.0f));
        __m256 code2_floats = _mm256_div_ps(_mm256_cvtepi32_ps(codes2_256), _mm256_set1_ps(255.0f));
        __m256 diff_values = _mm256_loadu_ps(diff + i);
        __m256 lower_bound_values = _mm256_loadu_ps(lower_bound + i);
        // Perform calculations
        __m256 scaled_codes1 =
            _mm256_add_ps(_mm256_mul_ps(code1_floats, diff_values), lower_bound_values);
        __m256 scaled_codes2 =
            _mm256_add_ps(_mm256_mul_ps(code2_floats, diff_values), lower_bound_values);
        __m256 val = _mm256_mul_ps(scaled_codes1, scaled_codes2);
        sum = _mm256_add_ps(sum, val);
    }

    // Horizontal addition
    __m128 sum_high = _mm256_extractf128_ps(sum, 1);
    __m128 sum_low = _mm256_castps256_ps128(sum);
    __m128 sum_final = _mm_add_ps(sum_low, sum_high);
    sum_final = _mm_hadd_ps(sum_final, sum_final);
    sum_final = _mm_hadd_ps(sum_final, sum_final);

    // Extract the result from the register
    float result;
    _mm_store_ss(&result, sum_final);

    result += sse::SQ8ComputeCodesIP(codes1 + i, codes2 + i, lower_bound + i, diff + i, dim - i);
    return result;
#else
    return sse::SQ8ComputeCodesIP(codes1, codes2, lower_bound, diff, dim);
#endif
}

float
SQ8ComputeCodesL2Sqr(const uint8_t* codes1,
                     const uint8_t* codes2,
                     const float* lower_bound,
                     const float* diff,
                     uint64_t dim) {
#if defined(ENABLE_AVX)
    __m256 sum = _mm256_setzero_ps();
    uint64_t i = 0;
    for (; i + 7 < dim; i += 8) {
        // Load data into registers
        __m256i code1_values = load_8_char_and_convert(codes1 + i);
        __m256i code2_values = load_8_char_and_convert(codes2 + i);
        __m256 codes1_floats =
            _mm256_div_ps(_mm256_cvtepi32_ps(code1_values), _mm256_set1_ps(255.0f));
        __m256 codes2_floats =
            _mm256_div_ps(_mm256_cvtepi32_ps(code2_values), _mm256_set1_ps(255.0f));
        __m256 diff_values = _mm256_loadu_ps(diff + i);
        __m256 lower_bound_values = _mm256_loadu_ps(lower_bound + i);
        // Perform calculations
        __m256 scaled_codes1 =
            _mm256_add_ps(_mm256_mul_ps(codes1_floats, diff_values), lower_bound_values);
        __m256 scaled_codes2 =
            _mm256_add_ps(_mm256_mul_ps(codes2_floats, diff_values), lower_bound_values);
        __m256 val = _mm256_sub_ps(scaled_codes1, scaled_codes2);
        val = _mm256_mul_ps(val, val);
        sum = _mm256_add_ps(sum, val);
    }
    // Horizontal addition
    __m128 sum_high = _mm256_extractf128_ps(sum, 1);
    __m128 sum_low = _mm256_castps256_ps128(sum);
    __m128 sum_final = _mm_add_ps(sum_low, sum_high);
    sum_final = _mm_hadd_ps(sum_final, sum_final);
    sum_final = _mm_hadd_ps(sum_final, sum_final);
    // Extract the result from the register
    float result;
    _mm_store_ss(&result, sum_final);

    result += sse::SQ8ComputeCodesL2Sqr(codes1 + i, codes2 + i, lower_bound + i, diff + i, dim - i);
    return result;
#else
    return sse::SQ8ComputeCodesL2Sqr(codes1, codes2, lower_bound, diff, dim);
#endif
}

float
SQ4ComputeIP(const float* query,
             const uint8_t* codes,
             const float* lower_bound,
             const float* diff,
             uint64_t dim) {
    return sse::SQ4ComputeIP(query, codes, lower_bound, diff, dim);
}

float
SQ4ComputeL2Sqr(const float* query,
                const uint8_t* codes,
                const float* lower_bound,
                const float* diff,
                uint64_t dim) {
    return sse::SQ4ComputeL2Sqr(query, codes, lower_bound, diff, dim);
}

float
SQ4ComputeCodesIP(const uint8_t* codes1,
                  const uint8_t* codes2,
                  const float* lower_bound,
                  const float* diff,
                  uint64_t dim) {
    return sse::SQ4ComputeCodesIP(codes1, codes2, lower_bound, diff, dim);
}

float
SQ4ComputeCodesL2Sqr(const uint8_t* codes1,
                     const uint8_t* codes2,
                     const float* lower_bound,
                     const float* diff,
                     uint64_t dim) {
    return sse::SQ4ComputeCodesL2Sqr(codes1, codes2, lower_bound, diff, dim);
}

float
SQ4UniformComputeCodesIP(const uint8_t* codes1, const uint8_t* codes2, uint64_t dim) {
#if defined(ENABLE_AVX)
    return sse::SQ4UniformComputeCodesIP(codes1, codes2, dim);  // TODO(LHT): implement
#else
    return sse::SQ4UniformComputeCodesIP(codes1, codes2, dim);
#endif
}

float
SQ8UniformComputeCodesIP(const uint8_t* codes1, const uint8_t* codes2, uint64_t dim) {
#if defined(ENABLE_AVX)
    return sse::SQ8UniformComputeCodesIP(codes1, codes2, dim);  // TODO(LHT): implement
#else
    return sse::SQ8UniformComputeCodesIP(codes1, codes2, dim);
#endif
}

float
RaBitQFloatBinaryIP(const float* vector, const uint8_t* bits, uint64_t dim, float inv_sqrt_d) {
#if defined(ENABLE_AVX)
    if (dim == 0) {
        return 0.0f;
    }

    if (dim < 8) {
        return sse::RaBitQFloatBinaryIP(vector, bits, dim, inv_sqrt_d);
    }

    uint64_t d = 0;
    float result = 0.0f;
    alignas(32) float temp[8];
    __m256 sum = _mm256_setzero_ps();
    const __m256 inv_sqrt_d_vec = _mm256_set1_ps(inv_sqrt_d);

    for (; d + 8 <= dim; d += 8) {
        __m256 vec = _mm256_loadu_ps(vector + d);

        uint8_t byte = bits[d / 8];
        __m256 b_vec = _mm256_set_ps(((byte >> 7) & 1) ? 1.0f : -1.0f,
                                     ((byte >> 6) & 1) ? 1.0f : -1.0f,
                                     ((byte >> 5) & 1) ? 1.0f : -1.0f,
                                     ((byte >> 4) & 1) ? 1.0f : -1.0f,
                                     ((byte >> 3) & 1) ? 1.0f : -1.0f,
                                     ((byte >> 2) & 1) ? 1.0f : -1.0f,
                                     ((byte >> 1) & 1) ? 1.0f : -1.0f,
                                     ((byte >> 0) & 1) ? 1.0f : -1.0f);

        b_vec = _mm256_mul_ps(b_vec, inv_sqrt_d_vec);

        sum = _mm256_add_ps(_mm256_mul_ps(b_vec, vec), sum);
    }

    _mm256_store_ps(temp, sum);
    for (float val : temp) {
        result += val;
    }

    result += sse::RaBitQFloatBinaryIP(vector + d, bits + d / 8, dim - d, inv_sqrt_d);

    return result;
#else
    return sse::RaBitQFloatBinaryIP(vector, bits, dim, inv_sqrt_d);
#endif
}

void
DivScalar(const float* from, float* to, uint64_t dim, float scalar) {
#if defined(ENABLE_AVX)
    if (dim == 0) {
        return;
    }
    if (scalar == 0) {
        scalar = 1.0f;  // TODO(LHT): logger?
    }
    int i = 0;
    __m256 scalarVec = _mm256_set1_ps(scalar);
    for (; i + 7 < dim; i += 8) {
        __m256 vec = _mm256_loadu_ps(from + i);
        vec = _mm256_div_ps(vec, scalarVec);
        _mm256_storeu_ps(to + i, vec);
    }
    sse::DivScalar(from + i, to + i, dim - i, scalar);
#else
    sse::DivScalar(from, to, dim, scalar);
#endif
}

float
Normalize(const float* from, float* to, uint64_t dim) {
    float norm = std::sqrt(FP32ComputeIP(from, from, dim));
    avx::DivScalar(from, to, dim, norm);
    return norm;
}

}  // namespace vsag::avx
