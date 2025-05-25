#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cassert>
#include <cmath>
#include <vector>
#include <unordered_map>
#include <memory>

#include <clap/clap.h>

#include "plugin/plugin.h"
#include "plugin/PluginHost.h"
#include "gui/gui.h"
#include "audio.h"

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
}

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

namespace GuiExt {
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

	bool is_api_supported (const clap_plugin_t *plugin, const char *api, bool isFloating) {
		return Gui::IsApiSupported(toApiEnum(api), isFloating);
	}

	bool get_preferred_api(const clap_plugin_t *plugin, const char **apiString, bool *isFloating)  {
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
	}

	bool create(const clap_plugin_t *plugin, const char *apiString, bool isFloating) {
		Gui::WindowingApi api = toApiEnum(apiString);
		if (!Gui::IsApiSupported(api, isFloating)) {
			return false;
		}
		GetSharedState(plugin)->mGui = std::make_unique<Gui>(
				GetSharedState(plugin)->mHost.get(), api, isFloating);
		return true;
	}

	void destroy (const clap_plugin_t *plugin) {
		GetSharedState(plugin)->mGui.reset();
	}

	bool set_scale (const clap_plugin_t *plugin, double scale)  {
		return GetSharedState(plugin)->mGui->SetScale(scale);
	}

	bool get_size (const clap_plugin_t *plugin, uint32_t *width, uint32_t *height)  {
		return GetSharedState(plugin)->mGui->GetSize(*width, *height);
	}

	bool can_resize (const clap_plugin_t *plugin)  {
		return GetSharedState(plugin)->mGui->CanResize();
	}

	bool get_resize_hints (const clap_plugin_t *plugin, clap_gui_resize_hints_t *hints)  {
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
	}

	bool adjust_size (const clap_plugin_t *plugin, uint32_t *width, uint32_t *height)  {
		return GetSharedState(plugin)->mGui->AdjustSize(*width, *height);
	}

	bool set_size (const clap_plugin_t *plugin, uint32_t width, uint32_t height)  {
		return GetSharedState(plugin)->mGui->SetSize(width, height);
	}

	bool set_parent (const clap_plugin_t *plugin, const clap_window_t *window)  {
		Gui::WindowingApi api = toApiEnum(window->api);
		return GetSharedState(plugin)->mGui->SetParent(api, window->ptr);
	}

	bool set_transient (const clap_plugin_t *plugin, const clap_window_t *window)  {
		Gui::WindowingApi api = toApiEnum(window->api);
		return GetSharedState(plugin)->mGui->SetTransient(api, window->ptr);
	}

	void suggest_title (const clap_plugin_t *plugin, const char *title) {
		GetSharedState(plugin)->mGui->SuggestTitle(title);
	}

	bool show (const clap_plugin_t *plugin)  {
		return GetSharedState(plugin)->mGui->Show();
	}

	bool hide (const clap_plugin_t *plugin)  {
		return GetSharedState(plugin)->mGui->Hide();
	}

	const clap_plugin_gui_t value = {
		is_api_supported,
		get_preferred_api,
		create,
		destroy,
		set_scale,
		get_size,
		can_resize,
		get_resize_hints,
		adjust_size,
		set_size,
		set_parent,
		set_transient,
		suggest_title,
		show,
		hide,
	};
}

namespace TimerSupportExt {
	const clap_plugin_timer_support_t value = {
		[] (const clap_plugin *plugin, clap_id timer_id) {
			GetSharedState(plugin)->mHost->OnTimer(timer_id);
		}
	};
}

namespace Plugin {

	bool init(const clap_plugin *_plugin) {
		Gui::OnAppInit();
		return true;
	}

	void destroy(const clap_plugin *plugin) {
		auto* pState = GetSharedState(plugin);
		delete pState;
		Gui::OnAppQuit();
	}

	bool activate(const clap_plugin *plugin, double sampleRate, uint32_t minimumFramesCount, uint32_t maximumFramesCount) {
		auto* pState = GetSharedState(plugin);
		pState->sampleRate = sampleRate;
		return true;
	}

	void deactivate(const clap_plugin *_plugin) {
	}

	bool start_processing(const clap_plugin *_plugin) {
		return true;
	}

	void stop_processing(const clap_plugin *_plugin) {
	}

	void reset(const clap_plugin *_plugin) {
	}

	clap_process_status process(const clap_plugin *plugin, const clap_process_t *process) {
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
	}

	static const std::unordered_map<std::string_view, const void*> extensions = {
		{CLAP_EXT_NOTE_PORTS, &NotePortsExt::value},
		{CLAP_EXT_AUDIO_PORTS, &AudioPortsExt::value},
		{CLAP_EXT_GUI, &GuiExt::value},
		{CLAP_EXT_TIMER_SUPPORT, &TimerSupportExt::value},
	};

	const void* get_extension(const clap_plugin *plugin, const char *id) {
		if (auto search = extensions.find(id); search != extensions.end()) {
			return search->second;
		} else {
			return nullptr;
		}
	}

	void on_main_thread(const clap_plugin *_plugin) {
	}


    const clap_plugin_t* createPlugin(const clap_host_t *host) {
        static clap_plugin_t plugin = {
            &meta,
            /* plugin_data = */ nullptr,
            init,
            destroy,
            activate,
            deactivate,
            start_processing,
            stop_processing,
            reset,
            process,
            get_extension,
            on_main_thread,
        };

		SharedState *state = new SharedState();
		state->mHost = std::make_unique<PluginHost>(host);
		state->plugin = plugin;
		state->plugin.plugin_data = state;

		return &plugin;
    }
}
