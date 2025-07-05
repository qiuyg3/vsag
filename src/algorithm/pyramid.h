
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

#include <utility>

#include "base_filter_functor.h"
#include "data_cell/graph_interface.h"
#include "impl/basic_searcher.h"
#include "impl/odescent_graph_builder.h"
#include "index_feature_list.h"
#include "inner_index_interface.h"
#include "io/memory_io_parameter.h"
#include "logger.h"
#include "pyramid_zparameters.h"
#include "quantization/fp32_quantizer_parameter.h"
#include "safe_allocator.h"

namespace vsag {

class IndexNode;
using SearchFunc = std::function<MaxHeap(const IndexNode* node)>;

class IndexNode {
public:
    IndexNode(IndexCommonParam* common_param, GraphInterfaceParamPtr graph_param);

    void
    BuildGraph(ODescent& odescent);

    void
    InitGraph();

    MaxHeap
    SearchGraph(const SearchFunc& search_func) const;

    void
    AddChild(const std::string& key);

    std::shared_ptr<IndexNode>
    GetChild(const std::string& key, bool need_init = false);

    void
    Serialize(StreamWriter& writer) const;

    void
    Deserialize(StreamReader& reader);

public:
    GraphInterfacePtr graph_{nullptr};
    InnerIdType entry_point_{0};
    uint32_t level_{0};
    mutable std::mutex mutex_;

    Vector<InnerIdType> ids_;
    bool has_index_{false};

private:
    UnorderedMap<std::string, std::shared_ptr<IndexNode>> children_;
    IndexCommonParam* common_param_{nullptr};
    GraphInterfaceParamPtr graph_param_{nullptr};
};

class Pyramid : public InnerIndexInterface {
public:
    static ParamPtr
    CheckAndMappingExternalParam(const JsonType& external_param,
                                 const IndexCommonParam& common_param);

public:
    Pyramid(const PyramidParamPtr& pyramid_param, const IndexCommonParam& common_param)
        : InnerIndexInterface(pyramid_param, common_param),
          labels_(allocator_),
          pyramid_param_(pyramid_param),
          dim_(common_param.dim_),
          common_param_(common_param) {
        searcher_ = std::make_unique<BasicSearcher>(common_param_);
        flatten_interface_ptr_ =
            FlattenInterface::MakeInstance(pyramid_param_->flatten_data_cell_param, common_param_);
        root_ = std::make_shared<IndexNode>(&common_param_, pyramid_param_->graph_param);
        this->feature_list_ = std::make_shared<IndexFeatureList>();
        this->init_features();
    }

    explicit Pyramid(const ParamPtr& param, const IndexCommonParam& common_param)
        : Pyramid(std::dynamic_pointer_cast<PyramidParameters>(param), common_param){};

    std::string
    GetName() const override {
        return INDEX_PYRAMID;
    }

    ~Pyramid() = default;

    std::vector<int64_t>
    Build(const DatasetPtr& base) override;

    std::vector<int64_t>
    Add(const DatasetPtr& base) override;

    DatasetPtr
    KnnSearch(const DatasetPtr& query,
              int64_t k,
              const std::string& parameters,
              const FilterPtr& filter) const override;

    DatasetPtr
    RangeSearch(const DatasetPtr& query,
                float radius,
                const std::string& parameters,
                const FilterPtr& filter,
                int64_t limited_size = -1) const override;
    void
    Serialize(StreamWriter& writer) const override;

    void
    Deserialize(StreamReader& reader) override;

    int64_t
    GetNumElements() const override;

    int64_t
    GetMemoryUsage() const override;

    bool
    CheckFeature(IndexFeature feature) const override {
        return this->feature_list_->CheckFeature(feature);
    }

    void
    InitFeatures() override {
        this->init_features();
    }

private:
    void
    resize(int64_t new_max_capacity);

    void
    init_features();

    DatasetPtr
    search_impl(const DatasetPtr& query, int64_t limit, const SearchFunc& search_func) const;

    uint64_t
    cal_serialize_size() const;

private:
    IndexCommonParam common_param_;
    PyramidParamPtr pyramid_param_{nullptr};
    std::shared_ptr<IndexNode> root_{nullptr};
    FlattenInterfacePtr flatten_interface_ptr_{nullptr};
    LabelTable labels_;
    std::unique_ptr<VisitedListPool> pool_ = nullptr;
    std::unique_ptr<BasicSearcher> searcher_ = nullptr;
    int64_t max_capacity_{0};
    int64_t cur_element_count_{0};
    int64_t dim_{0};

    std::shared_mutex resize_mutex_;
    std::mutex cur_element_count_mutex_;

    IndexFeatureListPtr feature_list_{nullptr};
};

}  // namespace vsag
