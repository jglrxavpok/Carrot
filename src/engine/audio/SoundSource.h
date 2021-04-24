//
// Created by jglrxavpok on 18/04/2021.
//

#pragma once

#include "Sound.h"

namespace Carrot {
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

    class SoundSource: public enable_shared_from_this<SoundSource> {
    private:
        // TODO: gain, pitch controls
        constexpr static size_t BUFFERS_AT_ONCE = 10;

        CleanupPolicy cleanupPolicy = CleanupPolicy::OnSoundEnd;
        unique_ptr<Sound> currentSound = nullptr;
        AL::Source source{};
        bool looping = false;
        size_t queuedBufferCount = 0;
        float gain = 1.0f;

    public:
        explicit SoundSource();

        void play(unique_ptr<Sound>&& sound);

        void updateAudio();

        bool isPlaying() const { return source.isPlaying(); };

        bool isLooping() const { return looping; };

        void setLooping(bool loop) { this->looping = loop; };

        bool isReadyForCleanup();

        void setGain(float gain);

        float getGain() const { return gain; };

        ~SoundSource();
    };
}
