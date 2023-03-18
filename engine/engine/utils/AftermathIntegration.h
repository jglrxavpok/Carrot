//
// Created by jglrxavpok on 07/09/2022.
//

#pragma once

#include <string>

#ifdef AFTERMATH_ENABLE
void setAftermathMarker(vk::CommandBuffer& cmds, const std::string& markerData);

void initAftermath();
void shutdownAftermath();
#endif