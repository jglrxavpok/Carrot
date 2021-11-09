//
// Created by jglrxavpok on 08/11/2021.
//

#include "Executors.h"

namespace Carrot::Async {
    void StdAsyncExecutor::execute(const std::function<void()>& action) {
        futures.push_back(std::async(std::launch::async, action));
    }

    StdAsyncExecutor::~StdAsyncExecutor() {
        for (const auto& f : futures) {
            f.wait();
        }
    }
}