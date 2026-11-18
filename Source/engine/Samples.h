// The built-in transient set: short one-shots stored at 44.1 kHz / 16-bit.
// Every sample is rendered here from synthesis and seeded noise (no third-party or ROM content),
// so the set is identical on every machine and costs nothing on disk.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace df
{
constexpr double kTransientRate = 44100.0;

struct OneShot
{
    std::string name;
    std::vector<int16_t> data;
};

const std::vector<OneShot>& transientSet();
int numTransients();
} // namespace df
