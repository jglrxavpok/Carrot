//
// Created by jglrxavpok on 09/09/2023.
//

#include <core/utils/stringmanip.h>
#include "Music.h"

namespace Carrot::Audio {

    Music::Music(const Carrot::IO::Resource& audioFile): originatingResource(audioFile) {
        verify(audioFile.isFile(), "Non-file audio is not supported yet");
        auto path = GetVFS().resolve(Carrot::IO::VFS::Path { audioFile.getName() });
        filepath = Carrot::toString(path.u8string());

        pSourceSource = std::make_shared<SoundSource>();
        pSourceSource->setGain(1.0f);
    }

    std::unique_ptr<Sound> Music::createInstance() {
        return std::make_unique<Sound>(filepath, true /*streaming*/);
    }

    void Music::play() {
        if (!pSourceSource->isPlaying()) {
            pSourceSource->play(createInstance());
        }
    }

    void Music::setLooping(bool b) {
        pSourceSource->setLooping(b);
    }


} // Carrot::Audio