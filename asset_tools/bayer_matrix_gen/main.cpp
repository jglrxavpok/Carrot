//
// Created by jglrxavpok on 29/04/2023.
//

// https://en.wikipedia.org/wiki/Ordered_dithering

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image.h>
#include <stb_image_write.h>
#include <cstdint>

const int size = 8;

stbi_uc coefficients[] = {
        0 * 255 / 64,
        32 * 255 / 64,
        8 * 255 / 64,
        40 * 255 / 64,
        2 * 255 / 64,
        34 * 255 / 64,
        10 * 255 / 64,
        42 * 255 / 64,
        48 * 255 / 64,
        16 * 255 / 64,
        56 * 255 / 64,
        24 * 255 / 64,
        50 * 255 / 64,
        18 * 255 / 64,
        58 * 255 / 64,
        26 * 255 / 64,
        12 * 255 / 64,
        44 * 255 / 64,
        4 * 255 / 64,
        36 * 255 / 64,
        14 * 255 / 64,
        46 * 255 / 64,
        6 * 255 / 64,
        38 * 255 / 64,
        60 * 255 / 64,
        28 * 255 / 64,
        52 * 255 / 64,
        20 * 255 / 64,
        62 * 255 / 64,
        30 * 255 / 64,
        54 * 255 / 64,
        22 * 255 / 64,
        3 * 255 / 64,
        35 * 255 / 64,
        11 * 255 / 64,
        43 * 255 / 64,
        1 * 255 / 64,
        33 * 255 / 64,
        9 * 255 / 64,
        41 * 255 / 64,
        51 * 255 / 64,
        19 * 255 / 64,
        59 * 255 / 64,
        27 * 255 / 64,
        49 * 255 / 64,
        17 * 255 / 64,
        57 * 255 / 64,
        25 * 255 / 64,
        15 * 255 / 64,
        47 * 255 / 64,
        7 * 255 / 64,
        39 * 255 / 64,
        13 * 255 / 64,
        45 * 255 / 64,
        5 * 255 / 64,
        37 * 255 / 64,
        63 * 255 / 64,
        31 * 255 / 64,
        55 * 255 / 64,
        23 * 255 / 64,
        61 * 255 / 64,
        29 * 255 / 64,
        53 * 255 / 64,
        21 * 255 / 64,
};

int main() {
    stbi_uc* pixels = (stbi_uc*)STBIW_MALLOC(size*size);

    for(int y = 0; y < size; y++) {
        for(int x = 0; x < size; x++) {
            pixels[x + y * size] = coefficients[x + y * size];
        }
    }

    int success = stbi_write_png("dithering.png", size, size, 1, pixels, size * sizeof(stbi_uc));

    if(!success) {
        fprintf(stderr, "Failed to write file.\n");
        return -1;
    }

    printf("Done!");
    return 0;
}