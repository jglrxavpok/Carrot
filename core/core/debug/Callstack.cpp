//
// Created by jglrxavpok on 14/11/2026.
//

#include <core/debug/Callstack.h>
#include <stacktrace>

Carrot::Debug::Callstack::Callstack() {
    const std::stacktrace trace = std::stacktrace::current();
    for (i32 index = 0; index < trace.size() && index < MaxCallstackDepth; index++) {
        entries[index].filename = trace[index].source_file();
        entries[index].function = trace[index].description();
        entries[index].line = trace[index].source_line();
    }
}
