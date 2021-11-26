//
// Created by jglrxavpok on 17/04/2021.
//

#pragma once

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include "AL/al.h"
#include "AL/alext.h"
#include "AL/alc.h"

namespace AL {
    class Context;
    class Device;
    class Buffer;
    class Source;

    namespace {
        const char* getHumanReadableCode(ALenum error) {
            switch(error) {
                case AL_NO_ERROR: return "AL_NO_ERROR";
                case AL_INVALID_NAME: return "AL_INVALID_NAME";
                case AL_INVALID_ENUM: return "AL_INVALID_ENUM";
                case AL_INVALID_VALUE: return "AL_INVALID_VALUE";
                case AL_INVALID_OPERATION: return "AL_INVALID_OPERATION";
                case AL_OUT_OF_MEMORY: return "AL_OUT_OF_MEMORY";

                default: return "UNKNOWN";
            }
        }

        void checkALError() {
            ALenum error = alGetError();
            if(error != AL_NO_ERROR) {
                std::string humanReadableCode = getHumanReadableCode(error);
                throw std::runtime_error("OpenAL error: " + std::to_string(error)); // TODO: human readable name
            }
        }
    }

    class Context {
    private:
        ALCcontext* context = nullptr;

    public:
        Context(ALCcontext* context): context(context) {}

        operator ALCcontext*() { return context; };

        void makeCurrent() {
            alcMakeContextCurrent(context);
        }
    };

    class Device {
    private:
        ALCdevice* device = nullptr;

    public:
        Device(ALCdevice* device): device(device) {}

        operator ALCdevice*() { return device; };

        Context createContext() {
            return alcCreateContext(device, nullptr);
        }
    };

    class Buffer {
    private:
        ALuint buffer = -1;

    public:
        /**
          * Used to transfer ownership of buffer to this object, should not be used outside this class
          * @param buffer
          */
        explicit Buffer(ALuint buffer): buffer(buffer) {}

        explicit Buffer() {
            alGenBuffers(1, &buffer);
            checkALError();
        }

        void upload(ALenum format, ALuint freq, void* data, size_t size) {
            alBufferData(buffer, format, data, size, freq);
            checkALError();
        }

        ~Buffer() {
            alDeleteBuffers(1, &buffer);
            checkALError();
        }

        operator ALuint() { return buffer; };

        const ALuint& getALBuffer() const { return buffer; };

        static std::vector<std::unique_ptr<Buffer>> generateMultiple(size_t count) {
            std::vector<ALuint> ids;
            ids.resize(count);
            alGenBuffers(count, ids.data());
            checkALError();

            std::vector<std::unique_ptr<Buffer>> buffers;
            buffers.reserve(count);
            for (int i = 0; i < count; ++i) {
                buffers.emplace_back(std::make_unique<Buffer>(ids[i]));
            }

            return buffers;
        }
    };

    class Source {
    private:
        ALuint source = -1;
        std::vector<std::shared_ptr<Buffer>> queuedBuffers;

        /// Container to hold unqueued buffers, used to reduce the amount of allocations/deallocations while unqueuing buffers
        std::vector<ALuint> unqueueContainer;

    public:
        explicit Source() {
            alGenSources(1, &source);
            checkALError();
        }

        void queue(const std::shared_ptr<Buffer>& buffer) {
            alSourceQueueBuffers(source, 1, &buffer->getALBuffer());
            queuedBuffers.push_back(buffer);
        }

        void play() {
            alSourcePlay(source);
        }

        void pause() {
            alSourcePause(source);
        }

        void resume() {
            play();
        }

        void stop() {
            alSourceStop(source);
        }

        void unqueue(ALuint count) {
            if(unqueueContainer.size() < count) {
                unqueueContainer.resize(count);
            }
            alSourceUnqueueBuffers(source, count, unqueueContainer.data());
            queuedBuffers.erase(queuedBuffers.begin(), queuedBuffers.begin() + count);
        }

        size_t getQueuedBufferCount() const {
            return queuedBuffers.size();
        }

        bool isPlaying() const {
            ALenum state;
            alGetSourcei(source, AL_SOURCE_STATE, &state);
            return state == AL_PLAYING;
        }

        bool isPaused() const {
            ALenum state;
            alGetSourcei(source, AL_SOURCE_STATE, &state);
            return state == AL_PAUSED;
        }

        bool isStopped() const {
            ALenum state;
            alGetSourcei(source, AL_SOURCE_STATE, &state);
            return state == AL_STOPPED;
        }

        ALint getProcessedBufferCount() const {
            ALint count;
            alGetSourcei(source, AL_BUFFERS_PROCESSED, &count);
            return count;
        }

        operator ALuint() { return source; };

        ALuint getALSource() { return source; };

        void updateGain(float gain) {
            alSourcef(source, AL_GAIN, gain);
        }

        ~Source() {
            alDeleteSources(1, &source);
        }
    };

    inline Device openDefaultDevice() {
        return alcOpenDevice(nullptr);
    }
}