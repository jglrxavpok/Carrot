//
// Created by jglrxavpok on 19/04/2021.
//

#pragma once

#include <engine/audio/OpenAL.hpp>
#include "AudioDecoder.h"
#include "dr_wav.h"

namespace Carrot {
    using namespace std;

    class WavDecoder: public AudioDecoder {
    private:
        drwav wav;

    public:
        explicit WavDecoder(string filename);

        size_t getSampleCount() override;

        uint64_t getFrequency() override;

        vector<float> extractSamples(size_t sampleCount) override;

        ALenum getFormat() override;

        void seek(size_t sampleIndex) override;

        uint64_t getChannelCount() override;

        ~WavDecoder() override;
    };
}