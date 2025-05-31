/*
namespace NotePortsExt {
    uint32_t count(const clap_plugin_t *plugin, bool isInput) {
        return isInput ? 1 : 0;
    }

    bool get(const clap_plugin_t *plugin, uint32_t index, bool isInput, clap_note_port_info_t *info)   {
        if (!isInput || index) return false;
        info->id = 0;
        info->supported_dialects = CLAP_NOTE_DIALECT_CLAP;
        info->preferred_dialect = CLAP_NOTE_DIALECT_CLAP;
        snprintf(info->name, sizeof(info->name), "%s", "Note Port");
        return true;
    }

    const clap_plugin_note_ports_t value = {
        &count,
        &get,
    };
}
*/
