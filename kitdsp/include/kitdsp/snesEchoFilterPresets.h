#pragma once
#include "kitdsp/snesEcho.h"

namespace kitdsp {
namespace SNES {

enum class FilterType {
    Lowpass,
    Highpass,
    Bandpass,
    Other,
};

struct FilterPreset {
    const char* name;
    const char* description;
    FilterType type;
    float maxGainDb;
    uint8_t data[kFIRTaps];
};

static const size_t kNumFilterPresets = 4;
static const FilterPreset kFilterPresets[] = {
    {"None", "", FilterType::Other, 0.0f, {0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {"StandardHpf", "cutoff at 3khz", FilterType::Highpass, 0.91f, {0x58, 0xBF, 0xDB, 0xF0, 0xFE, 0x07, 0x0C, 0x0C}},
    {"StandardLpf", "cutoff at 5khz", FilterType::Lowpass, 0.65f, {0x0C, 0x21, 0x2B, 0x13, 0xFE, 0xF3, 0xF9}},
    {"StandardBpf",
     "passes between 1.5khz and 8.5khz",
     FilterType::Bandpass,
     0.53f,
     {0x34, 0x33, 0x00, 0xD9, 0xE5, 0x01, 0xFC, 0xEB}},
};
}  // namespace SNES
}  // namespace kitdsp