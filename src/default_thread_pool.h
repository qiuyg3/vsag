
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

#include <ThreadPool.h>

#include <functional>
#include <future>

#include "vsag/options.h"
#include "vsag/thread_pool.h"

namespace vsag {

class DefaultThreadPool : public ThreadPool {
public:
    explicit DefaultThreadPool(std::size_t threads);

    std::future<void>
    Enqueue(std::function<void(void)> task) override;

    void
    WaitUntilEmpty() override;

    void
    SetQueueSizeLimit(std::size_t limit) override;

    void
    SetPoolSize(std::size_t limit) override;

private:
    std::unique_ptr<progschj::ThreadPool> pool_;
};

}  // namespace vsag
