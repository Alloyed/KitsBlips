#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cassert>
#include <cmath>
#include <vector>
#include <unordered_map>
#include <memory>

#include "clap/clap.h"
#include "gui.h"
#include "audio.h"
#include "PluginHost.h"

/*
 * Adapted from
 * https://nakst.gitlab.io/tutorial/clap-part-1.html
 * but heavily modified to use C++ features
 */

namespace {
	struct SharedState {
		std::unique_ptr<Gui> mGui;
		std::unique_ptr<PluginHost> mHost;
		clap_plugin_t plugin;
		float sampleRate;
	};

	SharedState* GetSharedState(const clap_plugin_t* plugin) {
		// fixme this is definitely incorrect: casts away const, isn't careful about thread access, and so on
		SharedState* pState = (SharedState*)(plugin->plugin_data);
		return pState;
	}

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

	Gui::WindowingApi toApiEnum(const char* api) 
	{
		// clap promises that equal api types are the same pointer
		if(api == CLAP_WINDOW_API_X11) { return Gui::WindowingApi::X11; }
		if(api == CLAP_WINDOW_API_WAYLAND) { return Gui::WindowingApi::Wayland; }
		if(api == CLAP_WINDOW_API_WIN32) { return Gui::WindowingApi::Win32; }
		if(api == CLAP_WINDOW_API_COCOA) { return Gui::WindowingApi::Cocoa; }
		// should never happen
		return Gui::WindowingApi::None;
	}

	const clap_plugin_gui_t extensionGUI = {
		.is_api_supported = [] (const clap_plugin_t *plugin, const char *api, bool isFloating) -> bool {
			return Gui::IsApiSupported(toApiEnum(api), isFloating);
		},

		.get_preferred_api = [] (const clap_plugin_t *plugin, const char **apiString, bool *isFloating) -> bool {
			Gui::WindowingApi api;
			bool success = Gui::GetPreferredApi(api, *isFloating);
			if(!success){return false;}
			switch(api) {
				case Gui::WindowingApi::X11: { *apiString = CLAP_WINDOW_API_X11; break; }
				case Gui::WindowingApi::Wayland: { *apiString = CLAP_WINDOW_API_WAYLAND; break; }
				case Gui::WindowingApi::Win32: { *apiString = CLAP_WINDOW_API_WIN32; break; }
				case Gui::WindowingApi::Cocoa: { *apiString = CLAP_WINDOW_API_COCOA; break; }
				case Gui::WindowingApi::None: { return false; }
			}
			return true;
		},

		.create = [] (const clap_plugin_t *plugin, const char *apiString, bool isFloating) -> bool {
			Gui::WindowingApi api = toApiEnum(apiString);
			if (!Gui::IsApiSupported(api, isFloating)) {
				return false;
			}
			GetSharedState(plugin)->mGui = std::make_unique<Gui>(
				GetSharedState(plugin)->mHost.get(), api, isFloating);
			return true;
		},

		.destroy = [] (const clap_plugin_t *plugin) {
			GetSharedState(plugin)->mGui.reset();
		},

		.set_scale = [] (const clap_plugin_t *plugin, double scale) -> bool {
			return GetSharedState(plugin)->mGui->SetScale(scale);
		},

		.get_size = [] (const clap_plugin_t *plugin, uint32_t *width, uint32_t *height) -> bool {
			return GetSharedState(plugin)->mGui->GetSize(*width, *height);
		},

		.can_resize = [] (const clap_plugin_t *plugin) -> bool {
			return GetSharedState(plugin)->mGui->CanResize();
		},

		.get_resize_hints = [] (const clap_plugin_t *plugin, clap_gui_resize_hints_t *hints) -> bool {
			Gui::ResizeHints guiHints;
			bool success = GetSharedState(plugin)->mGui->GetResizeHints(guiHints);
			if(!success) {
				return false;
			}
			hints->can_resize_horizontally = guiHints.canResizeHorizontally;
			hints->can_resize_vertically = guiHints.canResizeVertically;
			hints->preserve_aspect_ratio = guiHints.preserveAspectRatio;
			hints->aspect_ratio_width = guiHints.aspectRatioWidth;
			hints->aspect_ratio_height = guiHints.aspectRatioHeight;
			return true;
		},

		.adjust_size = [] (const clap_plugin_t *plugin, uint32_t *width, uint32_t *height) -> bool {
			return GetSharedState(plugin)->mGui->AdjustSize(*width, *height);
		},

		.set_size = [] (const clap_plugin_t *plugin, uint32_t width, uint32_t height) -> bool {
			return GetSharedState(plugin)->mGui->SetSize(width, height);
		},

		.set_parent = [] (const clap_plugin_t *plugin, const clap_window_t *window) -> bool {
			Gui::WindowingApi api = toApiEnum(window->api);
			return GetSharedState(plugin)->mGui->SetParent(api, window->ptr);
		},

		.set_transient = [] (const clap_plugin_t *plugin, const clap_window_t *window) -> bool {
			Gui::WindowingApi api = toApiEnum(window->api);
			return GetSharedState(plugin)->mGui->SetTransient(api, window->ptr);
		},

		.suggest_title = [] (const clap_plugin_t *plugin, const char *title) {
			return GetSharedState(plugin)->mGui->SuggestTitle(title);
		},

		.show = [] (const clap_plugin_t *plugin) -> bool {
			return GetSharedState(plugin)->mGui->Show();
		},

		.hide = [] (const clap_plugin_t *plugin) -> bool {
			return GetSharedState(plugin)->mGui->Hide();
		},
	};

	const clap_plugin_timer_support_t extensionTimer = {
		.on_timer = [] (const clap_plugin *plugin, clap_id timer_id) {
			GetSharedState(plugin)->mHost->OnTimer(timer_id);
		}
	};

	const clap_plugin_t pluginClass = {
		.desc = &pluginDescriptor,
		.plugin_data = nullptr,

		.init = [] (const clap_plugin *_plugin) -> bool {
			Gui::OnAppInit();
			return true;
		},

		.destroy = [] (const clap_plugin *plugin) {
			auto* pState = GetSharedState(plugin);
			delete pState;
			Gui::OnAppQuit();
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
				Audio::Render(i, nextEventFrame, process->audio_outputs[0].data32[0], process->audio_outputs[0].data32[1]);
				i = nextEventFrame;
			}

			return CLAP_PROCESS_CONTINUE;
		},

		.get_extension = [] (const clap_plugin *plugin, const char *id) -> const void * {
			static const std::unordered_map<std::string_view, const void*> extensions = {
				{CLAP_EXT_NOTE_PORTS, &extensionNotePorts},
				{CLAP_EXT_AUDIO_PORTS, &extensionAudioPorts},
				{CLAP_EXT_GUI, &extensionGUI},
				{CLAP_EXT_TIMER_SUPPORT, &extensionTimer},
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
			if (!clap_version_is_compatible(host->clap_version) || std::string_view(pluginID) == pluginDescriptor.id) {
				return nullptr;
			}

			SharedState *state = new SharedState();
			state->mHost = std::make_unique<PluginHost>(host);
			state->plugin = pluginClass;
			state->plugin.plugin_data = state;
			return &state->plugin;
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
		return std::string_view(CLAP_PLUGIN_FACTORY_ID) == factoryID ? &pluginFactory : nullptr;
	},
};

