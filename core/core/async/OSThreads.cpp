//
// Created by jglrxavpok on 04/02/2022.
//

#include <core/Macros.h>
#include "OSThreads.h"

#ifdef _WIN32
#include <windows.h>

#elif __has_include(<unistd.h>)
#include <unistd.h>
#endif

namespace Carrot::Threads {
    // https://web.archive.org/web/20210506154303/https://social.msdn.microsoft.com/Forums/en-US/0f749fd8-8a43-4580-b54b-fbf964d68375/convert-stdstring-to-lpcwstr-best-way-in-c?forum=Vsexpressvc#f59dfd52-580d-43a7-849f-9519d858a8e9
#ifdef _WIN32
    std::wstring s2ws(std::string_view s) {
        int len;
        int slength = (int)s.length() + 1;
        len = MultiByteToWideChar(CP_ACP, 0, s.data(), slength, 0, 0);
        wchar_t* buf = new wchar_t[len];
        MultiByteToWideChar(CP_ACP, 0, s.data(), slength, buf, len);
        std::wstring r(buf);
        delete[] buf;
        return r;
    }
#endif

    void setName(std::thread& thread, std::string_view name) {
        // https://stackoverflow.com/questions/10121560/stdthread-naming-your-thread

#ifdef _MSC_VER
        std::wstring stemp = s2ws(name); // Temporary buffer is required
        LPCWSTR description = stemp.c_str();
        HRESULT hr = SetThreadDescription(static_cast<HANDLE>(reinterpret_cast<void*>(thread.native_handle())), description);
        verify(!FAILED(hr), "Failed to set thread name");
#elif __has_include(<unistd.h>)
        auto handle = thread.native_handle();
        pthread_setname_np(handle, name.data());
#else
#error "Don't know how to set thread name on this OS. Please fix."
#endif
    }
}