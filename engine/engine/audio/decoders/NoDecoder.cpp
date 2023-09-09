//
// Created by jglrxavpok on 09/09/2023.
//

#include <core/utils/Assert.h>
#include "NoDecoder.h"

namespace Carrot::Audio {
    NoDecoder::NoDecoder(ALenum format, std::vector<float> _samples, std::uint64_t frequency, std::uint64_t channelCount)
        : AudioDecoder("<no file>")
        , format(format)
        , frequency(frequency)
        , channelCount(channelCount)
    {
        pSamples = std::make_shared<std::vector<float>>(std::move(_samples));
        verify(channelCount > 0, "Channel count cannot be 0");
        sampleCount = pSamples->size() / channelCount;
    }

    ALenum NoDecoder::getFormat() {
        return format;
    }

    size_t NoDecoder::getSampleCount() {
        return sampleCount;
    }

    uint64_t NoDecoder::getFrequency() {
        return frequency;
    }

    uint64_t NoDecoder::getChannelCount() {
        return channelCount;
    }

    void NoDecoder::seek(size_t sampleIndex) {
        cursor = sampleIndex * channelCount;
    }

    std::vector<float> NoDecoder::extractSamples(size_t _sampleCount) {
        std::size_t readableSamples = std::min(_sampleCount * channelCount, pSamples->size() - cursor);

        std::vector<float> result;
        result.resize(readableSamples);
        verify(cursor + readableSamples <= pSamples->size(), "out of bounds read");
        memcpy(result.data(), pSamples->data() + cursor, readableSamples * sizeof(float));

        cursor += readableSamples;
        return std::move(result);
    }
} // Carrot::Audio