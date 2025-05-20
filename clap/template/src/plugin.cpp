#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cassert>
#include <cmath>
#include <vector>
#include <unordered_map>

#include "clap/clap.h"
#include "gui.h"

/*
 * Adapted from
 * https://nakst.gitlab.io/tutorial/clap-part-1.html
 * but heavily modified to use C++ features
 */

struct SharedState {
	Gui mGui;
	clap_plugin_t plugin;
	const clap_host_t *host;
	float sampleRate;
};

SharedState* GetSharedState(const clap_plugin_t* plugin) {
	// fixme this is definitely incorrect: casts away const, isn't careful about thread access, and so on
	SharedState* pState = (SharedState*)(plugin->plugin_data);
	return pState;
}

namespace {
	const clap_plugin_descriptor_t pluginDescriptor = {
		.clap_version = CLAP_VERSION_INIT,
		.id = "nakst.HelloCLAP",
		.name = "HelloCLAP",
		.vendor = "nakst",
		.url = "https://nakst.gitlab.io",
		.manual_url = "https://nakst.gitlab.io",
		.support_url = "https://nakst.gitlab.io",
		.version = "1.0.0",
		.description = "The best audio plugin ever.",

		.features = (const char *[]) {
			CLAP_PLUGIN_FEATURE_INSTRUMENT,
			CLAP_PLUGIN_FEATURE_SYNTHESIZER,
			CLAP_PLUGIN_FEATURE_STEREO,
			NULL,
		},
	};

	const clap_plugin_note_ports_t extensionNotePorts = {
		.count = [] (const clap_plugin_t *plugin, bool isInput) -> uint32_t {
			return isInput ? 1 : 0;
		},

		.get = [] (const clap_plugin_t *plugin, uint32_t index, bool isInput, clap_note_port_info_t *info) -> bool {
			if (!isInput || index) return false;
			info->id = 0;
			info->supported_dialects = CLAP_NOTE_DIALECT_CLAP;
			info->preferred_dialect = CLAP_NOTE_DIALECT_CLAP;
			snprintf(info->name, sizeof(info->name), "%s", "Note Port");
			return true;
		},
	};

	const clap_plugin_audio_ports_t extensionAudioPorts = {
		.count = [] (const clap_plugin_t *plugin, bool isInput) -> uint32_t { 
			return isInput ? 0 : 1; 
		},

		.get = [] (const clap_plugin_t *plugin, uint32_t index, bool isInput, clap_audio_port_info_t *info) -> bool {
			if (isInput || index) return false;
			info->id = 0;
			info->channel_count = 2;
			info->flags = CLAP_AUDIO_PORT_IS_MAIN;
			info->port_type = CLAP_PORT_STEREO;
			info->in_place_pair = CLAP_INVALID_ID;
			snprintf(info->name, sizeof(info->name), "%s", "Audio Output");
			return true;
		},
	};

	const clap_plugin_gui_t extensionGUI = {
		.is_api_supported = [] (const clap_plugin_t *plugin, const char *api, bool isFloating) -> bool {
			return Gui::API == api && !isFloating;
		},

		.get_preferred_api = [] (const clap_plugin_t *plugin, const char **api, bool *isFloating) -> bool {
			*api = Gui::API.data();
			*isFloating = false;
			return true;
		},

		.create = [] (const clap_plugin_t *plugin, const char *api, bool isFloating) -> bool {
			if (!extensionGUI.is_api_supported(plugin, api, isFloating)) return false;
			GetSharedState(plugin)->mGui.OnCreate();
			return true;
		},

		.destroy = [] (const clap_plugin_t *plugin) {
			GetSharedState(plugin)->mGui.OnDestroy();
		},

		.set_scale = [] (const clap_plugin_t *plugin, double scale) -> bool {
			return false;
		},

		.get_size = [] (const clap_plugin_t *plugin, uint32_t *width, uint32_t *height) -> bool {
			*width = Gui::WINDOW_WIDTH;
			*height = Gui::WINDOW_HEIGHT;
			return true;
		},

		.can_resize = [] (const clap_plugin_t *plugin) -> bool {
			return false;
		},

		.get_resize_hints = [] (const clap_plugin_t *plugin, clap_gui_resize_hints_t *hints) -> bool {
			return false;
		},

		.adjust_size = [] (const clap_plugin_t *plugin, uint32_t *width, uint32_t *height) -> bool {
			return extensionGUI.get_size(plugin, width, height);
		},

		.set_size = [] (const clap_plugin_t *plugin, uint32_t width, uint32_t height) -> bool {
			return true;
		},

		.set_parent = [] (const clap_plugin_t *plugin, const clap_window_t *window) -> bool {
			assert(Gui::API == window->api);
			GetSharedState(plugin)->mGui.SetParent();
			return true;
		},

		.set_transient = [] (const clap_plugin_t *plugin, const clap_window_t *window) -> bool {
			return false;
		},

		.suggest_title = [] (const clap_plugin_t *plugin, const char *title) {
		},

		.show = [] (const clap_plugin_t *plugin) -> bool {
			// We'll define GUISetVisible in our platform specific code file.
			GetSharedState(plugin)->mGui.OnShow();
			return true;
		},

		.hide = [] (const clap_plugin_t *plugin) -> bool {
			GetSharedState(plugin)->mGui.OnHide();
			return true;
		},
	};

	const clap_plugin_t pluginClass = {
		.desc = &pluginDescriptor,
		.plugin_data = nullptr,

		.init = [] (const clap_plugin *_plugin) -> bool {
			return true;
		},

		.destroy = [] (const clap_plugin *plugin) {
			auto* pState = GetSharedState(plugin);
			delete pState;
		},

		.activate = [] (const clap_plugin *plugin, double sampleRate, uint32_t minimumFramesCount, uint32_t maximumFramesCount) -> bool {
			auto* pState = GetSharedState(plugin);
			pState->sampleRate = sampleRate;
			return true;
		},

		.deactivate = [] (const clap_plugin *_plugin) {
		},

		.start_processing = [] (const clap_plugin *_plugin) -> bool {
			return true;
		},

		.stop_processing = [] (const clap_plugin *_plugin) {
		},

		.reset = [] (const clap_plugin *_plugin) {
		},

		.process = [] (const clap_plugin *plugin, const clap_process_t *process) -> clap_process_status {
			auto* pState = GetSharedState(plugin);

			assert(process->audio_outputs_count == 1);
			assert(process->audio_inputs_count == 0);

			const uint32_t frameCount = process->frames_count;
			const uint32_t inputEventCount = process->in_events->size(process->in_events);
			uint32_t eventIndex = 0;
			uint32_t nextEventFrame = inputEventCount ? 0 : frameCount;

			for (uint32_t i = 0; i < frameCount; ) {
				while (eventIndex < inputEventCount && nextEventFrame == i) {
					const clap_event_header_t *event = process->in_events->get(process->in_events, eventIndex);

					if (event->time != i) {
						nextEventFrame = event->time;
						break;
					}

					//PluginProcessEvent(pState, event);
					eventIndex++;

					if (eventIndex == inputEventCount) {
						nextEventFrame = frameCount;
						break;
					}
				}
				//audio::Render(i, nextEventFrame, process->audio_outputs[0].data32[0], process->audio_outputs[0].data32[1]);
				i = nextEventFrame;
			}

			return CLAP_PROCESS_CONTINUE;
		},

		.get_extension = [] (const clap_plugin *plugin, const char *id) -> const void * {
			static const std::unordered_map<std::string_view, const void*> extensions = {
				{CLAP_EXT_NOTE_PORTS, &extensionNotePorts},
				{CLAP_EXT_AUDIO_PORTS, &extensionAudioPorts},
				{CLAP_EXT_GUI, &extensionGUI},
			};
			if (auto search = extensions.find(id); search != extensions.end()) {
				return search->second;
			} else {
				return nullptr;
			}
		},

		.on_main_thread = [] (const clap_plugin *_plugin) {
		},
	};

	const clap_plugin_factory_t pluginFactory = {
		.get_plugin_count = [] (const clap_plugin_factory *factory) -> uint32_t { 
			return 1; 
		},

		.get_plugin_descriptor = [] (const clap_plugin_factory *factory, uint32_t index) -> const clap_plugin_descriptor_t * { 
			return index == 0 ? &pluginDescriptor : nullptr; 
		},

		.create_plugin = [] (const clap_plugin_factory *factory, const clap_host_t *host, const char *pluginID) -> const clap_plugin_t * {
			if (!clap_version_is_compatible(host->clap_version) || strcmp(pluginID, pluginDescriptor.id)) {
				return nullptr;
			}

			SharedState *plugin = new SharedState();
			plugin->host = host;
			plugin->plugin = pluginClass;
			plugin->plugin.plugin_data = plugin;
			return &plugin->plugin;
		},
	};
}

extern "C" const clap_plugin_entry_t clap_entry = {
	.clap_version = CLAP_VERSION_INIT,

	.init = [] (const char *path) -> bool { 
		return true; 
	},

	.deinit = [] () {},

	.get_factory = [] (const char *factoryID) -> const void * {
		return strcmp(factoryID, CLAP_PLUGIN_FACTORY_ID) ? nullptr : &pluginFactory;
	},
};

