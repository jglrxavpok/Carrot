//
// Created by jglrxavpok on 18/04/2021.
//

#pragma once

#include <memory>
#include <string>
#include <stdexcept>
#include "OpenAL.hpp"
#include "decoders/AudioDecoder.h"

namespace Carrot {
    class Sound {
    private:
        constexpr static size_t SAMPLES_AT_ONCE = 44100;
        bool endOfFile = false;
        bool streaming = false;
        std::unique_ptr<AudioDecoder> decoder = nullptr;

    public:
        Sound(const std::string& filename, bool streaming);

        std::shared_ptr<AL::Buffer> getNextBuffer();

        void rewind();

        void seek(size_t sampleIndex);

        bool hasBeenFullyRead() const {
            return endOfFile;
        };

        static std::unique_ptr<Sound> loadSoundEffect(const std::string& filename);

        static std::unique_ptr<Sound> loadMusic(const std::string& filename);
    };
}
