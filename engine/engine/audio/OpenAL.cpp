//
// Created by jglrxavpok on 09/09/2023.
//

#include "OpenAL.hpp"

namespace AL {
    Source::Source() {
        alGenSources(1, &source);
        checkALError();
    }

    void Source::queue(const std::shared_ptr<Buffer>& buffer) {
        alSourceQueueBuffers(source, 1, &buffer->getALBuffer());
        queuedBuffers.push_back(buffer);
    }

    void Source::removeAllBuffers() {
        alSourcei(source, AL_BUFFER, 0);
    }

    void Source::play() {
        alSourcePlay(source);
    }

    void Source::pause() {
        alSourcePause(source);
    }

    void Source::resume() {
        play();
    }

    void Source::stop() {
        alSourceStop(source);
    }

    void Source::unqueue(ALuint count) {
        if(unqueueContainer.size() < count) {
            unqueueContainer.resize(count);
        }
        alSourceUnqueueBuffers(source, count, unqueueContainer.data());
        queuedBuffers.erase(queuedBuffers.begin(), queuedBuffers.begin() + count);
    }

    size_t Source::getQueuedBufferCount() const {
        return queuedBuffers.size();
    }

    bool Source::isPlaying() const {
        ALenum state;
        alGetSourcei(source, AL_SOURCE_STATE, &state);
        return state == AL_PLAYING;
    }

    bool Source::isPaused() const {
        ALenum state;
        alGetSourcei(source, AL_SOURCE_STATE, &state);
        return state == AL_PAUSED;
    }

    bool Source::isStopped() const {
        ALenum state;
        alGetSourcei(source, AL_SOURCE_STATE, &state);
        return state == AL_STOPPED;
    }

    ALint Source::getProcessedBufferCount() const {
        ALint count;
        alGetSourcei(source, AL_BUFFERS_PROCESSED, &count);
        return count;
    }

    Source::operator ALuint() { return source; };

    ALuint Source::getALSource() { return source; };

    void Source::updateGain(float gain) {
        alSourcef(source, AL_GAIN, gain);
    }

    void Source::updatePitch(float pitch) {
        alSourcef(source, AL_PITCH, pitch);
    }

    void Source::setPosition(const glm::vec3& position) {
        alSource3f(source, AL_POSITION, position.x, position.y, position.z);
    }

    Source::~Source() {
        alDeleteSources(1, &source);
    }
}