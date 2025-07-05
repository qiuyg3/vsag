
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

#include <exceptions.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <variant>

#include "vsag/allocator.h"
#include "vsag/dataset.h"

namespace vsag {

class DatasetImpl : public Dataset {
    using var = std::variant<int64_t,
                             const float*,
                             const int8_t*,
                             const int64_t*,
                             const std::string*,
                             const SparseVector*>;

public:
    DatasetImpl() = default;

    ~DatasetImpl() override {
        if (not owner_) {
            return;
        }

        if (allocator_) {
            allocator_->Deallocate((void*)this->GetIds());
            allocator_->Deallocate((void*)this->GetDistances());
            allocator_->Deallocate((void*)this->GetInt8Vectors());
            allocator_->Deallocate((void*)this->GetFloat32Vectors());
            allocator_->Deallocate((void*)this->GetPaths());
            allocator_->Deallocate((void*)this->GetExtraInfos());

            if (this->GetSparseVectors()) {
                for (int i = 0; i < this->GetNumElements(); i++) {
                    allocator_->Deallocate((void*)this->GetSparseVectors()[i].ids_);
                    allocator_->Deallocate((void*)this->GetSparseVectors()[i].vals_);
                }
                allocator_->Deallocate((void*)this->GetSparseVectors());
            }

        } else {
            delete[] this->GetIds();
            delete[] this->GetDistances();
            delete[] this->GetInt8Vectors();
            delete[] this->GetFloat32Vectors();
            delete[] this->GetPaths();
            delete[] this->GetExtraInfos();

            if (this->GetSparseVectors()) {
                for (int i = 0; i < this->GetNumElements(); i++) {
                    delete[] this->GetSparseVectors()[i].ids_;
                    delete[] this->GetSparseVectors()[i].vals_;
                }
                delete[] this->GetSparseVectors();
            }
        }
    }

    DatasetImpl(const DatasetImpl&) = delete;
    DatasetImpl&
    operator=(const DatasetImpl&) = delete;

    DatasetImpl(DatasetImpl&& other) noexcept {
        this->owner_ = other.owner_;
        other.owner_ = false;
        this->data_ = other.data_;
        other.data_.clear();
    }

    DatasetPtr
    Owner(bool is_owner, Allocator* allocator) override {
        this->owner_ = is_owner;
        this->allocator_ = allocator;
        return shared_from_this();
    }

public:
    DatasetPtr
    NumElements(int64_t num_elements) override {
        this->data_[NUM_ELEMENTS] = num_elements;
        return shared_from_this();
    }

    int64_t
    GetNumElements() const override {
        if (auto iter = this->data_.find(NUM_ELEMENTS); iter != this->data_.end()) {
            return std::get<int64_t>(iter->second);
        }

        return 0;
    }

    DatasetPtr
    Dim(int64_t dim) override {
        this->data_[DIM] = dim;
        return shared_from_this();
    }

    int64_t
    GetDim() const override {
        if (auto iter = this->data_.find(DIM); iter != this->data_.end()) {
            return std::get<int64_t>(iter->second);
        }

        return 0;
    }

    DatasetPtr
    Ids(const int64_t* ids) override {
        this->data_[IDS] = ids;
        return shared_from_this();
    }

    const int64_t*
    GetIds() const override {
        if (auto iter = this->data_.find(IDS); iter != this->data_.end()) {
            return std::get<const int64_t*>(iter->second);
        }

        return nullptr;
    }

    DatasetPtr
    Distances(const float* dists) override {
        this->data_[DISTS] = dists;
        return shared_from_this();
    }

    const float*
    GetDistances() const override {
        if (auto iter = this->data_.find(DISTS); iter != this->data_.end()) {
            return std::get<const float*>(iter->second);
        }

        return nullptr;
    }

    DatasetPtr
    Int8Vectors(const int8_t* vectors) override {
        this->data_[INT8_VECTORS] = vectors;
        return shared_from_this();
    }

    const int8_t*
    GetInt8Vectors() const override {
        if (auto iter = this->data_.find(INT8_VECTORS); iter != this->data_.end()) {
            return std::get<const int8_t*>(iter->second);
        }

        return nullptr;
    }

    DatasetPtr
    Float32Vectors(const float* vectors) override {
        this->data_[FLOAT32_VECTORS] = vectors;
        return shared_from_this();
    }

    const float*
    GetFloat32Vectors() const override {
        if (auto iter = this->data_.find(FLOAT32_VECTORS); iter != this->data_.end()) {
            return std::get<const float*>(iter->second);
        }

        return nullptr;
    }

    DatasetPtr
    SparseVectors(const SparseVector* sparse_vectors) override {
        this->data_[SPARSE_VECTORS] = sparse_vectors;
        return shared_from_this();
    }

    const SparseVector*
    GetSparseVectors() const override {
        if (auto iter = this->data_.find(SPARSE_VECTORS); iter != this->data_.end()) {
            return std::get<const SparseVector*>(iter->second);
        }

        return nullptr;
    }

    DatasetPtr
    Paths(const std::string* paths) override {
        this->data_[DATASET_PATHS] = paths;
        return shared_from_this();
    }

    const std::string*
    GetPaths() const override {
        if (auto iter = this->data_.find(DATASET_PATHS); iter != this->data_.end()) {
            return std::get<const std::string*>(iter->second);
        }
        return nullptr;
    }

    DatasetPtr
    ExtraInfos(const char* extra_info) override {
        this->data_[EXTRA_INFOS] = reinterpret_cast<const int64_t*>(extra_info);
        return shared_from_this();
    }

    const char*
    GetExtraInfos() const override {
        if (auto iter = this->data_.find(EXTRA_INFOS); iter != this->data_.end()) {
            return reinterpret_cast<const char*>(std::get<const int64_t*>(iter->second));
        }
        return nullptr;
    }

    DatasetPtr
    ExtraInfoSize(int64_t extra_info_size) override {
        this->data_[EXTRA_INFO_SIZE] = extra_info_size;
        return shared_from_this();
    }

    int64_t
    GetExtraInfoSize() const override {
        if (auto iter = this->data_.find(EXTRA_INFO_SIZE); iter != this->data_.end()) {
            return std::get<int64_t>(iter->second);
        }
        return 0;
    }

    static DatasetPtr
    MakeEmptyDataset();

private:
    bool owner_ = true;
    std::unordered_map<std::string, var> data_;
    Allocator* allocator_ = nullptr;
};

};  // namespace vsag
