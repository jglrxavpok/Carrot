//
// Created by jglrxavpok on 18/06/2021.
//

#pragma once

namespace Carrot::IO {
    enum class FileFormat {
        PNG,
        JPEG,
    };

    constexpr bool isImageFormat(FileFormat format) {
        switch (format) {
            case FileFormat::PNG:
            case FileFormat::JPEG:
                return true;

            default: return false;
        }
    }
}