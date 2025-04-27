//
// Created by jglrxavpok on 25/04/25.
//

#include <mutex> // workaround for an issue (bug?) where MSVC manages to fail to compile mutex when included from one
// of the following headers. Of course it worked on the previous

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"