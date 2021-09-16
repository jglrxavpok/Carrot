//
// Created by jglrxavpok on 22/04/2021.
//

#include "VorbisDecoder.h"

Carrot::VorbisDecoder::VorbisDecoder(const std::string& filename) : AudioDecoder(filename) {
    int error = 0;
    vorbis = stb_vorbis_open_filename(this->filename.c_str(), &error, nullptr);
    info = stb_vorbis_get_info(vorbis);
}

size_t Carrot::VorbisDecoder::getSampleCount() {
    return stb_vorbis_stream_length_in_samples(vorbis);
}

uint64_t Carrot::VorbisDecoder::getFrequency() {
    return info.sample_rate;
}

std::vector<float> Carrot::VorbisDecoder::extractSamples(size_t sampleCount) {
    std::vector<float> result{};
    size_t totalSamples = sampleCount*getChannelCount();
    result.resize(totalSamples);
    unsigned int read = stb_vorbis_get_samples_float_interleaved(vorbis, getChannelCount(), result.data(), totalSamples);
    result.resize(read);
    return result;
}

ALenum Carrot::VorbisDecoder::getFormat() {
    if(getChannelCount() == 1) {
        return AL_FORMAT_MONO_FLOAT32;
    }
    return AL_FORMAT_STEREO_FLOAT32;
}

void Carrot::VorbisDecoder::seek(size_t sampleIndex) {
    stb_vorbis_seek_frame(vorbis, sampleIndex);
}

std::uint64_t Carrot::VorbisDecoder::getChannelCount() {
    return info.channels;
}

Carrot::VorbisDecoder::~VorbisDecoder() {
    stb_vorbis_close(vorbis);
}
