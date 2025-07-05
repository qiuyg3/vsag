
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

#include "graph_interface_parameter.h"
#include "io/io_parameter.h"

namespace vsag {
class GraphDataCellParameter : public GraphInterfaceParameter {
public:
    GraphDataCellParameter() = default;

    void
    FromJson(const JsonType& json) override;

    JsonType
    ToJson() override;

public:
    IOParamPtr io_parameter_{nullptr};

    uint64_t max_degree_{64};

    uint64_t init_max_capacity_{100};
};

using GraphDataCellParamPtr = std::shared_ptr<GraphDataCellParameter>;
}  // namespace vsag
