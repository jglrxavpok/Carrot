#include <cstdio>
#include "engine/Engine.h"
#include "engine/io/IO.h"
#include "engine/audio/OpenAL.hpp"
#include "engine/audio/Sound.h"
#include "engine/audio/SoundSource.h"
#include "dr_wav.h"

namespace Game {
    class Game: public SwapchainAware {
    public:
        explicit Game(Carrot::Engine& engine);

        void onFrame(uint32_t frameIndex);

        void tick(double frameTime);

        void recordGBufferPass(uint32_t frameIndex, vk::CommandBuffer& commands);

        void onMouseMove(double dx, double dy);

        void changeGraphicsWaitSemaphores(uint32_t frameIndex, vector<vk::Semaphore>& semaphores, vector<vk::PipelineStageFlags>& waitStages);

        void onSwapchainSizeChange(int newWidth, int newHeight) override;

        void onSwapchainImageCountChange(size_t newCount) override;
    };
}
Game::Game::Game(Carrot::Engine& engine) {}
void Game::Game::onFrame(uint32_t frameIndex) {}
void Game::Game::tick(double frameTime) {}
void Game::Game::recordGBufferPass(uint32_t frameIndex, vk::CommandBuffer& commands) {}
void Game::Game::onMouseMove(double dx, double dy) {}
void Game::Game::changeGraphicsWaitSemaphores(uint32_t frameIndex, vector<vk::Semaphore>& semaphores, vector<vk::PipelineStageFlags>& waitStages) {}
void Game::Game::onSwapchainSizeChange(int newWidth, int newHeight) {};
void Game::Game::onSwapchainImageCountChange(size_t newCount) {};

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
        source->play(Sound::loadMusic("resources/musics/Space Fighter Loop.mp3"));
        while(source->isPlaying());
    }
//    std::this_thread::sleep_for(std::chrono::milliseconds(5000));

    return 0;
}