//
// Created by jglrxavpok on 18/04/2021.
//

#pragma once

#include "Sound.h"

namespace Carrot::Audio {
    enum class CleanupPolicy {
        /**
         * Clean sound resources manually
         */
        Explicit,

        /**
         * Clean sound resources once the sound has finished playing
         */
        OnSoundEnd,
    };

    class SoundSource: public std::enable_shared_from_this<SoundSource> {
    private:
        // TODO: gain, pitch controls
        constexpr static size_t BUFFERS_AT_ONCE = 10;

        CleanupPolicy cleanupPolicy = CleanupPolicy::OnSoundEnd;
        std::unique_ptr<Sound> currentSound = nullptr;
        AL::Source source{};
        bool looping = false;
        std::size_t queuedBufferCount = 0;
        float gain = 1.0f;
        float pitch = 1.0f;

    public:
        explicit SoundSource();

        void play(std::unique_ptr<Sound>&& sound);

        void stop();

        void updateAudio();

        bool isPlaying() const { return source.isPlaying(); };

        bool isLooping() const { return looping; };

        void setLooping(bool loop) { this->looping = loop; };

        bool isReadyForCleanup();

        void setGain(float gain);
        void setPitch(float pitch);

        void setPosition(const glm::vec3& position);

        float getGain() const { return gain; }
        float getPitch() const { return pitch; }

        ~SoundSource();
    };
}
