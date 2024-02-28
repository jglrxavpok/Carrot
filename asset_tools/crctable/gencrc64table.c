//
// Created by jglrxavpok on 27/02/2024.
//

// Based on explanations at https://www.zlib.net/crc_v3.txt
// re-implemented by myself to ensure I understand the algorithm

#include <stdint.h>
#include <stdio.h>

/**
 * \brief Reflects bottom B bits of v
 */
uint64_t reflect(uint64_t v, int b) {
    uint64_t r = 0;
    for (int i = 0; i < b; i++) {
        if(v & (1ull << i))
            r |= 1ull << (b-1-i);
    }
    return r;
}

int main() {
    printf("// Generated from crctable/gencrc64table.c\n");
    printf("std::uint64_t CRCTable[256] = {\n\t");
    const uint64_t topBit = 1ull << 63;
    const uint64_t poly = 0xC96C5795D7870F42ULL; // CRC-64 poly, ECMA 182
    for(uint64_t index = 0; index < 256; index++) {
        uint64_t reg = (reflect(index, 8) << (64-8));

        // long (manual) division, but base 2, and with XOR arithmetics
        // idea: after 8 iterations of division (one byte of message to CRC),
        //  the multiple XOR applied to top byte and following byte can be collapsed into a single one (which we are computing below)
        for(int bit = 0; bit < 8; bit++) {
            if(reg & topBit) {
                // substraction is a XOR, so if topBit is set, we need to substract
                reg = (reg << 1ull) ^ poly;
            } else {
                reg <<= 1ull;
            }
        }
        reg = reflect(reg, 64);

        printf("0x%llxULL", reg);

        if((index+1) % 8 == 0) {
            printf(",\n\t");
        } else {
            printf(", ");
        }
    }
    printf("};\n");
    return 0;
}