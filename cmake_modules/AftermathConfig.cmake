find_path(AFTERMATH_INCLUDES NAMES
        GFSDK_Aftermath.h
        GFSDK_Aftermath_Defines.h
        GFSDK_Aftermath_GpuCrashDump.h
        GFSDK_Aftermath_GpuCrashDumpDecoding.h
)

find_library(AFTERMATH_LIBS NAMES
        GFSDK_Aftermath_Lib.${ARCH}
        )
