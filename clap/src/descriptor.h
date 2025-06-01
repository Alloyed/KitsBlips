#pragma once

#include <clap/clap.h>

static constexpr const char* const audio_effect_features[] = {
    CLAP_PLUGIN_FEATURE_AUDIO_EFFECT,
    nullptr,
};
inline constexpr clap_plugin_descriptor_t AudioEffectDescriptor(const char* id,
                                                                const char* name,
                                                                const char* description) {
    clap_plugin_descriptor_t descriptor{};
    descriptor.clap_version = CLAP_VERSION;
    descriptor.id = id;
    descriptor.name = name;
    descriptor.vendor = "KitsBlips";
    descriptor.url = "https://alloyed.me/KitsBlips";
    descriptor.manual_url = "https://alloyed.me/KitsBlips";
    descriptor.support_url = "https://alloyed.me/KitsBlips";
    descriptor.version = "0.0.0";
    descriptor.description = description;
    descriptor.features = audio_effect_features;

    return descriptor;
}