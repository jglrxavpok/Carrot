#include <cstdio>
#include "engine/Engine.h"
#include "engine/io/IO.h"
#include "engine/audio/OpenAL.hpp"
#include "engine/audio/Sound.h"
#include "engine/audio/SoundSource.h"
#include "engine/CarrotGame.h"
#include "dr_wav.h"

namespace Game {
    class Game: public Carrot::CarrotGame {
    public:
        explicit Game(Carrot::Engine& engine): Carrot::CarrotGame(engine) {};

        void onFrame(uint32_t frameIndex) override {};

        void tick(double frameTime) override {};

        void recordGBufferPass(Carrot::Render::Context renderContext, vk::CommandBuffer& commands) override {};

        void onMouseMove(double dx, double dy) override {};

        void changeGraphicsWaitSemaphores(uint32_t frameIndex, vector<vk::Semaphore>& semaphores, vector<vk::PipelineStageFlags>& waitStages) override {};

        void onSwapchainSizeChange(int newWidth, int newHeight) override {};

        void onSwapchainImageCountChange(size_t newCount) override {};
    };
}

int main() {
    {
        auto coinPCM = IO::readFile("resources/sounds/coin.wav");

        auto alDevice = AL::openDefaultDevice();
        auto context = alDevice.createContext();
        context.makeCurrent();

        drwav wav;
        if (!drwav_init_memory(&wav, coinPCM.data(), coinPCM.size(), nullptr)) {

        }

        float *pDecodedInterleavedPCMFrames = (float *) malloc(wav.totalPCMFrameCount * wav.channels * sizeof(float));
        size_t numberOfSamplesActuallyDecoded = drwav_read_pcm_frames_f32(&wav, wav.totalPCMFrameCount,
                                                                          pDecodedInterleavedPCMFrames);

        ALenum format = AL_FORMAT_MONO_FLOAT32;
        if (wav.channels == 1) {
            format = AL_FORMAT_MONO_FLOAT32;
        } else {
            format = AL_FORMAT_STEREO_FLOAT32;
        }
        auto buffer = make_shared<AL::Buffer>();
        buffer->upload(format, wav.sampleRate, pDecodedInterleavedPCMFrames,
                       numberOfSamplesActuallyDecoded * wav.channels * sizeof(float));

        AL::Source source;
        source.queue(buffer);

        source.play();
        while (source.isPlaying());

        source.unqueue(source.getProcessedBufferCount());

        auto buffer2 = make_shared<AL::Buffer>();
        buffer2->upload(format, wav.sampleRate, pDecodedInterleavedPCMFrames,
                        numberOfSamplesActuallyDecoded * wav.channels * sizeof(float));

        source.queue(buffer2);

        source.play();
        while (source.isPlaying());

        printf("hi %zu", numberOfSamplesActuallyDecoded);
    }

    using namespace Carrot;
    {
        shared_ptr<SoundSource> source = make_shared<SoundSource>();
     //   source->setLooping(true);
        source->play(Sound::loadSoundEffect("resources/sounds/coin.wav"));
        while(source->isPlaying());
    }

    {
        shared_ptr<SoundSource> source = make_shared<SoundSource>();
       // source->setLooping(true);
        source->play(Sound::loadMusic("resources/musics/mus_mettaton_ex.ogg"));
        source->setGain(0.3f);
        while(source->isPlaying());
    }
//    std::this_thread::sleep_for(std::chrono::milliseconds(5000));

    return 0;
}

void Carrot::Engine::initGame() {
    game = make_unique<Game::Game>(*this);
}