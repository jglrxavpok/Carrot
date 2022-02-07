//
// Created by jglrxavpok on 30/01/2022.
//

#pragma once

#include <atomic>
#include <compare>
#include <xhash>

namespace Carrot::Async {
    /// Represents different group on which a task can run
    class TaskLane {
    public:
        TaskLane();
        TaskLane(const TaskLane& lane) = default;

        auto operator<=>(const TaskLane& other) const {
            return id <=> other.id;
        }

        bool operator==(const TaskLane& other) const {
            return id == other.id;
        }

    public:
        /// Task is run during on the current thread
        static TaskLane Undefined;

    private:
        std::uint64_t id = 0;
        static std::atomic_uint64_t counter;

        friend class ::std::hash<Carrot::Async::TaskLane>;
    };

}