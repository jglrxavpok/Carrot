//
// Created by jglrxavpok on 18/04/2021.
//

#pragma once

#include <utility>
#include <vector>
#include <string>

namespace Carrot {
    using namespace std;

    class AudioDecoder {
    protected:
        string filename;

    public:
        explicit AudioDecoder(std::string filename): filename(std::move(filename)) {};

        virtual ALenum getFormat() = 0;
        virtual size_t getSampleCount() = 0;
        virtual uint64_t getFrequency() = 0;
        virtual uint64_t getChannelCount() = 0;
        virtual void seek(size_t sampleIndex) = 0;
        virtual vector<float> extractSamples(size_t sampleCount) = 0;
        virtual ~AudioDecoder() = default;
    };
}
