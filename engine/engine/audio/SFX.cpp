//
// Created by jglrxavpok on 09/09/2023.
//

#include <core/utils/stringmanip.h>
#include "SFX.h"

namespace Carrot::Audio {
    SFX::SFX(const Carrot::IO::Resource& audioFile): originatingResource(audioFile) {
        verify(audioFile.isFile(), "Non-file audio is not supported yet");

        verify(audioFile.isFile(), "Non file music not supported for now");
        auto path = GetVFS().resolve(Carrot::IO::VFS::Path { audioFile.getName() });
        Sound templateSound {Carrot::toString(path.u8string()), false /*streaming*/};
        decoderTemplate = templateSound.copyInMemory();
    }

    std::unique_ptr<Sound> SFX::createInstance() {
        return std::make_unique<Sound>(std::make_unique<NoDecoder>(decoderTemplate), false /* not streaming */);
    }

} // Carrot::Audio