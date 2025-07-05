
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

#include <chrono>

namespace vsag {
class LinearCongruentialGenerator {
public:
    LinearCongruentialGenerator() {
        auto now = std::chrono::steady_clock::now();
        auto timestamp =
            std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        current_ = static_cast<unsigned int>(timestamp);
    }

    float
    NextFloat() {
        current_ = (A * current_ + C) % M;
        return static_cast<float>(current_) / static_cast<float>(M);
    }

private:
    unsigned int current_;
    static const uint32_t A = 1664525;
    static const uint32_t C = 1013904223;
    static const uint32_t M = 4294967295;  // 2^32 - 1
};
}  // namespace vsag
