//
// Created by jglrxavpok on 29/11/2020.
//

#include <mutex> // workaround for an issue (bug?) where MSVC manages to fail to compile mutex when included from one
// of the following headers. Of course it worked on the previous

#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

#define DR_MP3_IMPLEMENTATION
#include "dr_mp3.h"

#include "stb_vorbis.c"

#define VMA_IMPLEMENTATION
#define VMA_DEBUG_LOG_FORMAT(format, ...) do { \
printf((format), __VA_ARGS__); \
printf("\n"); \
} while(false)
#include <vk_mem_alloc.h>
