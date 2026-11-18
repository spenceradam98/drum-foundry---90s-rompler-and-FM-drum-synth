// Factory kits (16 kits x 8 pads) and the voice-preset library you can load onto any pad.
#pragma once

#include <string>
#include <vector>

#include "Params.h"

namespace df
{
struct VoicePreset
{
    std::string name;
    VoiceParams p;
};

const std::vector<Kit>& factoryKits();
const std::vector<VoicePreset>& voicePresets();
} // namespace df
