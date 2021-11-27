//
// Created by jglrxavpok on 18/09/2021.
//
#include "Coroutines.hpp"

namespace Carrot::Async {
    void DeferringAwaiter::transferOwnership(Task<>&& task) {
        ownedTasks.emplace_back(std::move(task));
    }

    void DeferringAwaiter::updateOwnedCompletedTasks() {
        for(auto& ownedTask : ownedTasks) {
            ownedTask();
        }
        ownedTasks.erase(std::remove_if(WHOLE_CONTAINER(ownedTasks), [](const auto& t) { return t.done(); }), ownedTasks.end());
    }
}
