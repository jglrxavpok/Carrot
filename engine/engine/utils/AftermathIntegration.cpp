#ifdef _WIN32
    #include <windows.h>
#endif
#include "NsightAftermathHelpers.h"
#include <fstream>
#include <iostream>
#include <mutex>

static std::mutex mut{};

static void WriteGpuCrashDumpToFile(const void* data, const std::uint32_t size) {
    auto output = std::ofstream("gpucrash.nv-gpudmp", std::ios::out | std::ios::binary);
    output.write(static_cast<const char *>(data), size);
    output.flush();
    output.close();
}

static void GpuCrashDumpCallback(const void* pGpuCrashDump, const uint32_t gpuCrashDumpSize, void* pUserData)
{
    std::lock_guard lk(mut);
    WriteGpuCrashDumpToFile(pGpuCrashDump, gpuCrashDumpSize);
}

static void OnDescription(PFN_GFSDK_Aftermath_AddGpuCrashDumpDescription addDescription, void* pUserData)
{
    // Add some basic description about the crash.
    addDescription(GFSDK_Aftermath_GpuCrashDumpDescriptionKey_ApplicationName, "Carrot");
    addDescription(GFSDK_Aftermath_GpuCrashDumpDescriptionKey_ApplicationVersion, "v0.0.1");
    addDescription(GFSDK_Aftermath_GpuCrashDumpDescriptionKey_UserDefined, "GPU Crash dump");
    addDescription(GFSDK_Aftermath_GpuCrashDumpDescriptionKey_UserDefined + 1, "Test string, ignore.");
}

void initAftermath() {
    AFTERMATH_CHECK_ERROR(GFSDK_Aftermath_EnableGpuCrashDumps(
            GFSDK_Aftermath_Version_API,
            GFSDK_Aftermath_GpuCrashDumpWatchedApiFlags_Vulkan,
            GFSDK_Aftermath_GpuCrashDumpFeatureFlags_DeferDebugInfoCallbacks, // Let the Nsight Aftermath library cache shader debug information.
            GpuCrashDumpCallback,                                             // Register callback for GPU crash dumps.
            nullptr,                                          // Register callback for shader debug information.
            OnDescription,                                     // Register callback for GPU crash dump description.
            nullptr,                                            // Register callback for resolving application-managed markers.
            nullptr));                                                           // Set the GpuCrashTracker object as user data for the above callbacks.

}
