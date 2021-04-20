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
    using namespace std;

    class Sound {
    private:
        constexpr static size_t SAMPLES_AT_ONCE = 44100;
        bool endOfFile = false;
        bool streaming = false;
        unique_ptr<AudioDecoder> decoder = nullptr;

    public:
        Sound(const string& filename, bool streaming);

        shared_ptr<AL::Buffer> getNextBuffer();

        void rewind();

        void seek(size_t sampleIndex);

        bool hasBeenFullyRead() const {
            return endOfFile;
        };

        static unique_ptr<Sound> loadSoundEffect(const string& filename);

        static unique_ptr<Sound> loadMusic(const string& filename);
    };
}
