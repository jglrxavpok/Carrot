//
// Created by jglrxavpok on 08/11/2021.
//

#pragma once

#include <cstdint>
#include <cstddef>
#include <functional>
#include <future>

namespace Carrot::Async {
    class StdAsyncExecutor {
    public:
        void execute(const std::function<void()>& action);

        ~StdAsyncExecutor();

    private:
        std::list<std::future<void>> futures;
    };
}