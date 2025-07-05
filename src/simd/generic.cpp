
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

#include "simd.h"

namespace vsag::generic {

float
L2Sqr(const void* pVect1v, const void* pVect2v, const void* qty_ptr) {
    auto* pVect1 = (float*)pVect1v;
    auto* pVect2 = (float*)pVect2v;
    uint64_t qty = *((uint64_t*)qty_ptr);

    float res = 0.0f;
    for (uint64_t i = 0; i < qty; i++) {
        float t = *pVect1 - *pVect2;
        pVect1++;
        pVect2++;
        res += t * t;
    }
    return res;
}

float
InnerProduct(const void* pVect1, const void* pVect2, const void* qty_ptr) {
    uint64_t qty = *((uint64_t*)qty_ptr);
    float res = 0;
    for (unsigned i = 0; i < qty; i++) {
        res += ((float*)pVect1)[i] * ((float*)pVect2)[i];
    }
    return res;
}

float
InnerProductDistance(const void* pVect1, const void* pVect2, const void* qty_ptr) {
    return 1.0f - InnerProduct(pVect1, pVect2, qty_ptr);
}

float
INT8InnerProduct(const void* pVect1, const void* pVect2, const void* qty_ptr) {
    uint64_t qty = *((uint64_t*)qty_ptr);
    auto* vec1 = (int8_t*)pVect1;
    auto* vec2 = (int8_t*)pVect2;
    double res = 0;
    for (uint64_t i = 0; i < qty; i++) {
        res += vec1[i] * vec2[i];
    }
    return static_cast<float>(res);
}

float
INT8InnerProductDistance(const void* pVect1, const void* pVect2, const void* qty_ptr) {
    return -INT8InnerProduct(pVect1, pVect2, qty_ptr);
}

void
PQDistanceFloat256(const void* single_dim_centers, float single_dim_val, void* result) {
    const auto* float_centers = (const float*)single_dim_centers;
    auto* float_result = (float*)result;
    for (uint64_t idx = 0; idx < 256; idx++) {
        double diff = float_centers[idx] - single_dim_val;
        float_result[idx] += (float)(diff * diff);
    }
}

float
FP32ComputeIP(const float* query, const float* codes, uint64_t dim) {
    float result = 0.0f;

    for (uint64_t i = 0; i < dim; ++i) {
        result += query[i] * codes[i];
    }
    return result;
}

float
FP32ComputeL2Sqr(const float* query, const float* codes, uint64_t dim) {
    float result = 0.0f;
    for (uint64_t i = 0; i < dim; ++i) {
        auto val = query[i] - codes[i];
        result += val * val;
    }
    return result;
}

union FP32Struct {
    uint32_t int_value;
    float float_value;
};

float
BF16ToFloat(const uint16_t bf16_value) {
    FP32Struct fp32;
    fp32.int_value = (static_cast<uint32_t>(bf16_value) << 16);
    return fp32.float_value;
}

uint16_t
FloatToBF16(const float fp32_value) {
    FP32Struct fp32;
    fp32.float_value = fp32_value;
    return static_cast<uint16_t>((fp32.int_value + 0x8000) >> 16);
}

float
FP16ToFloat(const uint16_t fp16_value) {
    uint32_t sign = (fp16_value >> 15) & 0x1;
    int32_t exp = ((fp16_value >> 10) & 0x1F) - 15;
    uint32_t mantissa = (fp16_value & 0x3FF) << 13;
    FP32Struct fp32;
    fp32.int_value = (sign << 31) | ((exp + 127) << 23) | mantissa;
    return fp32.float_value;
}

uint16_t
FloatToFP16(const float fp32_value) {
    FP32Struct fp32;
    fp32.float_value = fp32_value;
    uint16_t sign = (fp32.int_value >> 31) & 0x1;
    int32_t exp = ((fp32.int_value >> 23) & 0xFF) - 127;
    uint32_t mantissa = fp32.int_value & 0x007FFFFF;

    if (exp > 15) {
        exp = 15;
    } else if (exp < -14) {
        exp = -14;
    }
    return (sign << 15) | ((exp + 15) << 10) | (mantissa >> 13);
}

float
BF16ComputeIP(const uint8_t* query, const uint8_t* codes, uint64_t dim) {
    float result = 0.0f;
    auto* query_bf16 = reinterpret_cast<const uint16_t*>(query);
    auto* codes_bf16 = reinterpret_cast<const uint16_t*>(codes);
    for (uint64_t i = 0; i < dim; ++i) {
        result += BF16ToFloat(query_bf16[i]) * BF16ToFloat(codes_bf16[i]);
    }
    return result;
}

float
BF16ComputeL2Sqr(const uint8_t* query, const uint8_t* codes, uint64_t dim) {
    float result = 0.0f;
    auto* query_bf16 = reinterpret_cast<const uint16_t*>(query);
    auto* codes_bf16 = reinterpret_cast<const uint16_t*>(codes);
    for (uint64_t i = 0; i < dim; ++i) {
        auto val = BF16ToFloat(query_bf16[i]) - BF16ToFloat(codes_bf16[i]);
        result += val * val;
    }
    return result;
}

float
FP16ComputeIP(const uint8_t* query, const uint8_t* codes, uint64_t dim) {
    float result = 0.0f;
    auto* query_bf16 = reinterpret_cast<const uint16_t*>(query);
    auto* codes_bf16 = reinterpret_cast<const uint16_t*>(codes);
    for (uint64_t i = 0; i < dim; ++i) {
        result += FP16ToFloat(query_bf16[i]) * FP16ToFloat(codes_bf16[i]);
    }
    return result;
}

float
FP16ComputeL2Sqr(const uint8_t* query, const uint8_t* codes, uint64_t dim) {
    float result = 0.0f;
    auto* query_bf16 = reinterpret_cast<const uint16_t*>(query);
    auto* codes_bf16 = reinterpret_cast<const uint16_t*>(codes);
    for (uint64_t i = 0; i < dim; ++i) {
        auto val = FP16ToFloat(query_bf16[i]) - FP16ToFloat(codes_bf16[i]);
        result += val * val;
    }
    return result;
}

float
SQ8ComputeIP(const float* query,
             const uint8_t* codes,
             const float* lower_bound,
             const float* diff,
             uint64_t dim) {
    float result = 0.0f;
    for (uint64_t i = 0; i < dim; ++i) {
        result += query[i] * static_cast<float>(static_cast<float>(codes[i]) / 255.0 * diff[i] +
                                                lower_bound[i]);
    }
    return result;
}

float
SQ8ComputeL2Sqr(const float* query,
                const uint8_t* codes,
                const float* lower_bound,
                const float* diff,
                uint64_t dim) {
    float result = 0.0f;
    for (uint64_t i = 0; i < dim; ++i) {
        auto val = (query[i] - static_cast<float>(static_cast<float>(codes[i]) / 255.0 * diff[i] +
                                                  lower_bound[i]));
        result += val * val;
    }
    return result;
}

float
SQ8ComputeCodesIP(const uint8_t* codes1,
                  const uint8_t* codes2,
                  const float* lower_bound,
                  const float* diff,
                  uint64_t dim) {
    float result = 0.0f;
    for (uint64_t i = 0; i < dim; ++i) {
        auto val1 =
            static_cast<float>(static_cast<float>(codes1[i]) / 255.0 * diff[i] + lower_bound[i]);
        auto val2 =
            static_cast<float>(static_cast<float>(codes2[i]) / 255.0 * diff[i] + lower_bound[i]);
        result += val1 * val2;
    }
    return result;
}

float
SQ8ComputeCodesL2Sqr(const uint8_t* codes1,
                     const uint8_t* codes2,
                     const float* lower_bound,
                     const float* diff,
                     uint64_t dim) {
    float result = 0.0f;
    for (uint64_t i = 0; i < dim; ++i) {
        auto val1 =
            static_cast<float>(static_cast<float>(codes1[i]) / 255.0 * diff[i] + lower_bound[i]);
        auto val2 =
            static_cast<float>(static_cast<float>(codes2[i]) / 255.0 * diff[i] + lower_bound[i]);
        result += (val1 - val2) * (val1 - val2);
    }
    return result;
}

float
SQ4ComputeIP(const float* query,
             const uint8_t* codes,
             const float* lower_bound,
             const float* diff,
             uint64_t dim) {
    float result = 0;
    float x_lo = 0, x_hi = 0, y_lo = 0, y_hi = 0;

    for (uint64_t d = 0; d < dim; d += 2) {
        x_lo = query[d];
        y_lo = (codes[d >> 1] & 0x0f) / 15.0 * diff[d] + lower_bound[d];
        if (d + 1 < dim) {
            x_hi = query[d + 1];
            y_hi = (codes[d >> 1] >> 4) / 15.0 * diff[d + 1] + lower_bound[d + 1];
        } else {
            x_hi = 0;
            y_hi = 0;
        }

        result += (x_lo * y_lo + x_hi * y_hi);
    }

    return result;
}

float
SQ4ComputeL2Sqr(const float* query,
                const uint8_t* codes,
                const float* lower_bound,
                const float* diff,
                uint64_t dim) {
    float result = 0;
    float x_lo = 0, x_hi = 0, y_lo = 0, y_hi = 0;

    for (uint64_t d = 0; d < dim; d += 2) {
        x_lo = query[d];
        y_lo = (codes[d >> 1] & 0x0f) / 15.0 * diff[d] + lower_bound[d];
        if (d + 1 < dim) {
            x_hi = query[d + 1];
            y_hi = ((codes[d >> 1] & 0xf0) >> 4) / 15.0 * diff[d + 1] + lower_bound[d + 1];
        } else {
            x_hi = 0;
            y_hi = 0;
        }

        result += (x_lo - y_lo) * (x_lo - y_lo) + (x_hi - y_hi) * (x_hi - y_hi);
    }

    return result;
}

float
SQ4ComputeCodesIP(const uint8_t* codes1,
                  const uint8_t* codes2,
                  const float* lower_bound,
                  const float* diff,
                  uint64_t dim) {
    float result = 0, delta = 0;
    float x_lo = 0, x_hi = 0, y_lo = 0, y_hi = 0;

    for (uint64_t d = 0; d < dim; d += 2) {
        x_lo = (codes1[d >> 1] & 0x0f) / 15.0 * diff[d] + lower_bound[d];
        y_lo = (codes2[d >> 1] & 0x0f) / 15.0 * diff[d] + lower_bound[d];
        if (d + 1 < dim) {
            x_hi = ((codes1[d >> 1] & 0xf0) >> 4) / 15.0 * diff[d] + lower_bound[d];
            y_hi = ((codes2[d >> 1] & 0xf0) >> 4) / 15.0 * diff[d] + lower_bound[d];
        } else {
            x_hi = 0;
            y_hi = 0;
        }

        result += (x_lo * y_lo + x_hi * y_hi);
    }

    return result;
}

float
SQ4ComputeCodesL2Sqr(const uint8_t* codes1,
                     const uint8_t* codes2,
                     const float* lower_bound,
                     const float* diff,
                     uint64_t dim) {
    float result = 0, delta = 0;
    float x_lo = 0, x_hi = 0, y_lo = 0, y_hi = 0;

    for (uint64_t d = 0; d < dim; d += 2) {
        x_lo = (codes1[d >> 1] & 0x0f) / 15.0 * diff[d] + lower_bound[d];
        y_lo = (codes2[d >> 1] & 0x0f) / 15.0 * diff[d] + lower_bound[d];
        if (d + 1 < dim) {
            x_hi = ((codes1[d >> 1] & 0xf0) >> 4) / 15.0 * diff[d] + lower_bound[d];
            y_hi = ((codes2[d >> 1] & 0xf0) >> 4) / 15.0 * diff[d] + lower_bound[d];
        } else {
            x_hi = 0;
            y_hi = 0;
        }

        result += (x_lo - y_lo) * (x_lo - y_lo) + (x_hi - y_hi) * (x_hi - y_hi);
    }

    return result;
}

float
SQ4UniformComputeCodesIP(const uint8_t* codes1, const uint8_t* codes2, uint64_t dim) {
    int32_t result = 0;

    for (uint64_t d = 0; d < dim; d += 2) {
        float x_lo = codes1[d >> 1] & 0x0f;
        float x_hi = (codes1[d >> 1] & 0xf0) >> 4;
        float y_lo = codes2[d >> 1] & 0x0f;
        float y_hi = (codes2[d >> 1] & 0xf0) >> 4;

        result += (x_lo * y_lo + x_hi * y_hi);
    }

    return result;
}

float
SQ8UniformComputeCodesIP(const uint8_t* codes1, const uint8_t* codes2, uint64_t dim) {
    int32_t result = 0;
    for (uint64_t d = 0; d < dim; d++) {
        result += codes1[d] * codes2[d];
    }
    return static_cast<float>(result);
}

float
RaBitQFloatBinaryIP(const float* vector, const uint8_t* bits, uint64_t dim, float inv_sqrt_d) {
    if (dim == 0) {
        return 0.0f;
    }

    float result = 0.0f;

    for (std::size_t d = 0; d < dim; ++d) {
        bool bit = ((bits[d / 8] >> (d % 8)) & 1) != 0;
        float b_i = bit ? inv_sqrt_d : -inv_sqrt_d;
        result += b_i * vector[d];
    }

    return result;
}

float
Normalize(const float* from, float* to, uint64_t dim) {
    float norm = std::sqrt(FP32ComputeIP(from, from, dim));
    generic::DivScalar(from, to, dim, norm);
    return norm;
}

float
NormalizeWithCentroid(const float* from, const float* centroid, float* to, uint64_t dim) {
    float norm = 0;
    for (uint64_t d = 0; d < dim; ++d) {
        norm += (from[d] - centroid[d]) * (from[d] - centroid[d]);
    }

    if (norm < 1e-5) {
        norm = 1;
    } else {
        norm = std::sqrt(norm);
    }

    for (int d = 0; d < dim; d++) {
        to[d] = (from[d] - centroid[d]) / norm;
    }

    return norm;
}

void
InverseNormalizeWithCentroid(
    const float* from, const float* centroid, float* to, uint64_t dim, float norm) {
    for (int d = 0; d < dim; d++) {
        to[d] = from[d] * norm + centroid[d];
    }
}

void
DivScalar(const float* from, float* to, uint64_t dim, float scalar) {
    if (dim == 0) {
        return;
    }
    if (scalar == 0) {
        scalar = 1.0f;  // TODO(LHT): logger?
    }
    for (uint64_t i = 0; i < dim; ++i) {
        to[i] = from[i] / scalar;
    }
}

void
Prefetch(const void* data){};

}  // namespace vsag::generic
