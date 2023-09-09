//
// Created by jglrxavpok on 18/04/2021.
//

#pragma once

#include <memory>
#include <string>
#include <stdexcept>
#include "OpenAL.hpp"
#include "decoders/AudioDecoder.h"
#include "decoders/NoDecoder.h"

namespace Carrot::Audio {
    /**
     * Represents a sound instance. Can be a music or a sound effect.
     */
    class Sound {
    private:
        constexpr static size_t SAMPLES_AT_ONCE = 44100;
        bool endOfFile = false;
        bool streaming = false;
        std::unique_ptr<AudioDecoder> decoder = nullptr;

    public:
        Sound(std::unique_ptr<AudioDecoder>&& decoder, bool streaming);
        Sound(const std::string& filename, bool streaming); // TODO: support for Carrot::IO::Resource

        std::shared_ptr<AL::Buffer> getNextBuffer();

        void rewind();

        void seek(size_t sampleIndex);

        bool hasBeenFullyRead() const {
            return endOfFile;
        }

        NoDecoder copyInMemory();
    };
}
