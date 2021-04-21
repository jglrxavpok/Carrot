//
// Created by jglrxavpok on 21/04/2021.
//

#pragma once

#include <engine/audio/OpenAL.hpp>
#include "dr_mp3.h"
#include "AudioDecoder.h"

namespace Carrot {
    class MP3Decoder: public AudioDecoder {
    private:
        drmp3 mp3;

    public:
        explicit MP3Decoder(string filename);

        size_t getSampleCount() override;

        uint64_t getFrequency() override;

        vector<float> extractSamples(size_t sampleCount) override;

        ALenum getFormat() override;

        void seek(size_t sampleIndex) override;

        uint64_t getChannelCount() override;

        ~MP3Decoder() override;
    };
}
