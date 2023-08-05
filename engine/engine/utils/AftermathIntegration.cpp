// Copyright from NVidia Aftermath samples:
// https://github.com/NVIDIA/nsight-aftermath-samples/blob/6f985096067e4efdb0b0d0b78f2e64dacd591798/VkHelloNsightAftermath/NsightAftermathGpuCrashTracker.cpp
//*********************************************************
//
// Copyright (c) 2019-2022, NVIDIA CORPORATION. All rights reserved.
//
//  Permission is hereby granted, free of charge, to any person obtaining a
//  copy of this software and associated documentation files (the "Software"),
//  to deal in the Software without restriction, including without limitation
//  the rights to use, copy, modify, merge, publish, distribute, sublicense,
//  and/or sell copies of the Software, and to permit persons to whom the
//  Software is furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
//  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
//  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
//  DEALINGS IN THE SOFTWARE.
//
//*********************************************************
#include <engine/render/resources/Buffer.h>
#include "rapidjson/document.h"
#include "rapidjson/filewritestream.h"
#include "rapidjson/prettywriter.h"
#include "engine/render/raytracing/AccelerationStructure.h"
#include <fstream>
#include <iostream>
#include <mutex>

#ifdef _WIN32
    #include <windows.h>
#endif

#ifdef AFTERMATH_ENABLE
#include <GFSDK_Aftermath_GpuCrashDump.h>
#include "engine/utils/NsightAftermathHelpers.h"
#endif

static std::mutex mut{};
static bool enabled = false;
static std::mutex markersAccess;
static std::vector<std::string> markers;

//! shader hash -> shader debug data
static std::unordered_map<std::string, std::vector<std::uint8_t>> shaderDebug;

static void OnDescription(PFN_GFSDK_Aftermath_AddGpuCrashDumpDescription addDescription, void* pUserData)
{
    // Add some basic description about the crash.
    addDescription(GFSDK_Aftermath_GpuCrashDumpDescriptionKey_ApplicationName, "Carrot");
    addDescription(GFSDK_Aftermath_GpuCrashDumpDescriptionKey_ApplicationVersion, "v0.0.1");
    addDescription(GFSDK_Aftermath_GpuCrashDumpDescriptionKey_UserDefined, "GPU Crash dump");
    addDescription(GFSDK_Aftermath_GpuCrashDumpDescriptionKey_UserDefined + 1, "Test string, ignore.");
}

static void ResolveMarker(const void* pMarker, void* pUserData, void** resolvedMarkerData, uint32_t* markerSize) {
    std::lock_guard lk(markersAccess);
    auto& str = markers[(std::uint64_t)pMarker];
    *resolvedMarkerData = (void*)str.data();
    *markerSize = static_cast<std::uint32_t>(str.size());
}

static void RegisterShaderDebug(const void* pShaderDebugInfo, const uint32_t shaderDebugInfoSize, void* pUserData) {
    std::lock_guard l{ markersAccess };

    GFSDK_Aftermath_ShaderDebugInfoIdentifier identifier = {};
    AFTERMATH_CHECK_ERROR(GFSDK_Aftermath_GetShaderDebugInfoIdentifier(
            GFSDK_Aftermath_Version_API,
            pShaderDebugInfo,
            shaderDebugInfoSize,
            &identifier));

    std::vector<uint8_t> data((uint8_t*)pShaderDebugInfo, (uint8_t*)pShaderDebugInfo + shaderDebugInfoSize);
    std::string str = std::to_string(identifier);
    shaderDebug[str].swap(data);

    // Create a unique file name.
    const std::string filePath = "shader-" + str + ".nvdbg";

    std::ofstream f(filePath, std::ios::out | std::ios::binary);
    if (f) {
        f.write((const char*)pShaderDebugInfo, shaderDebugInfoSize);
    }
}

static void ShaderDebugLookup(const GFSDK_Aftermath_ShaderDebugInfoIdentifier* pIdentifier,
                              PFN_GFSDK_Aftermath_SetData setShaderDebugInfo,
                              void* pUserData) {
    std::string key = std::to_string(*pIdentifier);
    auto it = shaderDebug.find(key);
    if(it == shaderDebug.end()) {
        return;
    }

    setShaderDebugInfo(it->second.data(), static_cast<std::uint32_t>(it->second.size()));
}

static void WriteGpuCrashDumpToFile(const void* pGpuCrashDump, const uint32_t gpuCrashDumpSize) {
    // Create a GPU crash dump decoder object for the GPU crash dump.
    GFSDK_Aftermath_GpuCrashDump_Decoder decoder = {};
    AFTERMATH_CHECK_ERROR(GFSDK_Aftermath_GpuCrashDump_CreateDecoder(
            GFSDK_Aftermath_Version_API,
            pGpuCrashDump,
            gpuCrashDumpSize,
            &decoder));

    // Use the decoder object to read basic information, like application
    // name, PID, etc. from the GPU crash dump.
    GFSDK_Aftermath_GpuCrashDump_BaseInfo baseInfo = {};
    AFTERMATH_CHECK_ERROR(GFSDK_Aftermath_GpuCrashDump_GetBaseInfo(decoder, &baseInfo));

    GFSDK_Aftermath_GpuCrashDump_PageFaultInfo pageFaultInfo = {};
    AFTERMATH_CHECK_ERROR(GFSDK_Aftermath_GpuCrashDump_GetPageFaultInfo(decoder, &pageFaultInfo));


    // Use the decoder object to query the application name that was set
    // in the GPU crash dump description.
    uint32_t applicationNameLength = 0;
    AFTERMATH_CHECK_ERROR(GFSDK_Aftermath_GpuCrashDump_GetDescriptionSize(
            decoder,
            GFSDK_Aftermath_GpuCrashDumpDescriptionKey_ApplicationName,
            &applicationNameLength));

    std::vector<char> applicationName(applicationNameLength, '\0');

    AFTERMATH_CHECK_ERROR(GFSDK_Aftermath_GpuCrashDump_GetDescription(
            decoder,
            GFSDK_Aftermath_GpuCrashDumpDescriptionKey_ApplicationName,
            uint32_t(applicationName.size()),
            applicationName.data()));

    // Create a unique file name for writing the crash dump data to a file.
    // Note: due to an Nsight Aftermath bug (will be fixed in an upcoming
    // driver release) we may see redundant crash dumps. As a workaround,
    // attach a unique count to each generated file name.
    static int count = 0;
    const std::string baseFileName =
            std::string(applicationName.data())
            + "-"
            + std::to_string(baseInfo.pid)
            + "-"
            + std::to_string(++count);

    // Write the crash dump data to a file using the .nv-gpudmp extension
    // registered with Nsight Graphics.
    const std::string crashDumpFileName = baseFileName + ".nv-gpudmp";
    std::ofstream dumpFile(crashDumpFileName, std::ios::out | std::ios::binary);
    if (dumpFile)
    {
        dumpFile.write((const char*)pGpuCrashDump, gpuCrashDumpSize);
        dumpFile.close();
    }

    // Decode the crash dump to a JSON string.
    // Step 1: Generate the JSON and get the size.
    uint32_t jsonSize = 0;
    AFTERMATH_CHECK_ERROR(GFSDK_Aftermath_GpuCrashDump_GenerateJSON(
            decoder,
            GFSDK_Aftermath_GpuCrashDumpDecoderFlags_ALL_INFO,
            GFSDK_Aftermath_GpuCrashDumpFormatterFlags_UTF8_OUTPUT,
            ShaderDebugLookup,
            nullptr,
            nullptr, // TODO: shader source debug info lookup
            nullptr,
            &jsonSize));
    // Step 2: Allocate a buffer and fetch the generated JSON.
    std::vector<char> json(jsonSize);
    AFTERMATH_CHECK_ERROR(GFSDK_Aftermath_GpuCrashDump_GetJSON(
            decoder,
            uint32_t(json.size()),
            json.data()));

    rapidjson::Document jsonDocument;
    jsonDocument.Parse(json.data(), json.size());

    auto baseArray = jsonDocument.GetArray();

    rapidjson::Value resourcesObject{rapidjson::kObjectType};
    rapidjson::Value resourcesArray{rapidjson::kArrayType};

    for(const auto& [address, buffer] : Carrot::Buffer::BufferByStartAddress.snapshot()) {
        rapidjson::Value resource{rapidjson::kObjectType};
        resource.AddMember("Type", "Buffer", jsonDocument.GetAllocator());
        resource.AddMember("Name", (*buffer)->getDebugName(), jsonDocument.GetAllocator());
        resource.AddMember("Start Address", address, jsonDocument.GetAllocator());
        resource.AddMember("Size", (*buffer)->getSize(), jsonDocument.GetAllocator());
        resource.AddMember("Range", Carrot::sprintf("%x - %x", address, address + (*buffer)->getSize()), jsonDocument.GetAllocator());
        resource.AddMember("Range (decimal)", Carrot::sprintf("%llu - %llu", address, address + (*buffer)->getSize()), jsonDocument.GetAllocator());
        resourcesArray.PushBack(resource, jsonDocument.GetAllocator());
    }

    for(const auto& [address, as] : Carrot::AccelerationStructure::ASByStartAddress.snapshot()) {
        rapidjson::Value resource{rapidjson::kObjectType};
        resource.AddMember("Type", "AS", jsonDocument.GetAllocator());
        resource.AddMember("Start Address", address, jsonDocument.GetAllocator());
        resource.AddMember("Buffer", (*as)->getBuffer().getDeviceAddress(), jsonDocument.GetAllocator());
        resourcesArray.PushBack(resource, jsonDocument.GetAllocator());
    }

    resourcesObject.AddMember("Resources", resourcesArray, jsonDocument.GetAllocator());
    baseArray.PushBack(resourcesObject, jsonDocument.GetAllocator());

    // Write the crash dump data as JSON to a file.
    const std::string jsonFileName = crashDumpFileName + ".json";
    FILE* fp = fopen(jsonFileName.c_str(), "wb"); // non-Windows use "w"

    char writeBuffer[65536];
    rapidjson::FileWriteStream os(fp, writeBuffer, sizeof(writeBuffer));

    rapidjson::PrettyWriter<rapidjson::FileWriteStream> writer(os);

    jsonDocument.Accept(writer);
    fclose(fp);

    // Destroy the GPU crash dump decoder object.
    AFTERMATH_CHECK_ERROR(GFSDK_Aftermath_GpuCrashDump_DestroyDecoder(decoder));
}

static void GpuCrashDumpCallback(const void* pGpuCrashDump, const uint32_t gpuCrashDumpSize, void* pUserData)
{
    std::lock_guard lk(mut);
    WriteGpuCrashDumpToFile(pGpuCrashDump, gpuCrashDumpSize);
}

void setAftermathMarker(vk::CommandBuffer& cmds, const std::string& markerData) {
    if(!enabled) {
        return;
    }

    std::lock_guard lk(markersAccess);

    cmds.setCheckpointNV((const void*)markers.size());
    markers.push_back(markerData);
}

void initAftermath() {
    AFTERMATH_CHECK_ERROR(GFSDK_Aftermath_EnableGpuCrashDumps(
            GFSDK_Aftermath_Version_API,
            GFSDK_Aftermath_GpuCrashDumpWatchedApiFlags_Vulkan,
            GFSDK_Aftermath_GpuCrashDumpFeatureFlags_DeferDebugInfoCallbacks, // Let the Nsight Aftermath library cache shader debug information.
            GpuCrashDumpCallback,                                             // Register callback for GPU crash dumps.
            RegisterShaderDebug,                                          // Register callback for shader debug information.
            OnDescription,                                     // Register callback for GPU crash dump description.
            ResolveMarker,                                            // Register callback for resolving application-managed markers.
            nullptr));                                                           // Set the GpuCrashTracker object as user data for the above callbacks.

    enabled = true;
}

void shutdownAftermath() {
    AFTERMATH_CHECK_ERROR(GFSDK_Aftermath_DisableGpuCrashDumps());
}
