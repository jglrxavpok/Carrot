//
// Created by jglrxavpok on 19/04/2021.
//

#include "WavDecoder.h"

#include <utility>
#include <stdexcept>

Carrot::WavDecoder::WavDecoder(const std::string& filename): AudioDecoder(filename), wav() {
    if(!drwav_init_file(&wav, this->filename.c_str(), nullptr)) {
        throw std::runtime_error("Failed to open WAV file: "+filename);
    }
}

size_t Carrot::WavDecoder::getSampleCount() {
    return wav.totalPCMFrameCount;
}

uint64_t Carrot::WavDecoder::getFrequency() {
    return wav.sampleRate;
}

std::vector<float> Carrot::WavDecoder::extractSamples(size_t sampleCount) {
    std::vector<float> buffer;
    buffer.resize(sampleCount * sizeof(float) * wav.channels);
    auto read = drwav_read_pcm_frames_f32(&wav, sampleCount, buffer.data());
    buffer.resize(read);
    return buffer;
}

void Carrot::WavDecoder::seek(size_t sampleIndex) {
    if(!drwav_seek_to_pcm_frame(&wav, sampleIndex)) {
        throw std::runtime_error("Failed to seek.");
    }
}

ALenum Carrot::WavDecoder::getFormat() {
    if(wav.channels == 1) {
        return AL_FORMAT_MONO_FLOAT32;
    }
    return AL_FORMAT_STEREO_FLOAT32;
}

uint64_t Carrot::WavDecoder::getChannelCount() {
    return wav.channels;
}

Carrot::WavDecoder::~WavDecoder() {
    drwav_uninit(&wav);
}
