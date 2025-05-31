/*
namespace AudioPortsExt {
    uint32_t count(const clap_plugin_t *plugin, bool isInput) { 
        return isInput ? 0 : 1; 
    }

    bool get(const clap_plugin_t *plugin, uint32_t index, bool isInput, clap_audio_port_info_t *info) {
        if (isInput || index) return false;
        info->id = 0;
        info->channel_count = 2;
        info->flags = CLAP_AUDIO_PORT_IS_MAIN;
        info->port_type = CLAP_PORT_STEREO;
        info->in_place_pair = CLAP_INVALID_ID;
        snprintf(info->name, sizeof(info->name), "%s", "Audio Output");
        return true;
    }

    const clap_plugin_audio_ports_t value = {
        &count,
        &get,
    };
}
*/
