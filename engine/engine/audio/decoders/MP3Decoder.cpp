//
// Created by jglrxavpok on 21/04/2021.
//

#include "MP3Decoder.h"

#include <utility>
#include <stdexcept>

Carrot::MP3Decoder::MP3Decoder(const std::string& filename): AudioDecoder(filename), mp3() {
    if(!drmp3_init_file(&mp3, this->filename.c_str(), nullptr)) {
        throw std::runtime_error("Failed to open MP3 file: " + filename);
    }
}

size_t Carrot::MP3Decoder::getSampleCount() {
    return drmp3_get_pcm_frame_count(&mp3);
}

uint64_t Carrot::MP3Decoder::getFrequency() {
    return mp3.sampleRate;
}

std::vector<float> Carrot::MP3Decoder::extractSamples(size_t sampleCount) {
    std::vector<float> buffer;
    buffer.resize(sampleCount * mp3.channels);
    auto read = drmp3_read_pcm_frames_f32(&mp3, sampleCount, buffer.data());
    buffer.resize(read);
    return buffer;
}

void Carrot::MP3Decoder::seek(size_t sampleIndex) {
    if(!drmp3_seek_to_pcm_frame(&mp3, sampleIndex)) {
        throw std::runtime_error("Failed to seek.");
    }
}

ALenum Carrot::MP3Decoder::getFormat() {
    if(mp3.channels == 1) {
        return AL_FORMAT_MONO_FLOAT32;
    }
    return AL_FORMAT_STEREO_FLOAT32;
}

uint64_t Carrot::MP3Decoder::getChannelCount() {
    return mp3.channels;
}

Carrot::MP3Decoder::~MP3Decoder() {
    drmp3_uninit(&mp3);
}
