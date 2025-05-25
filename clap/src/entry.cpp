/* Main clap entry point. enumerate plugins here! */

#include <clap/clap.h>
#include <memory>

#include "plugin/PluginHost.h"
#include "plugin/plugin.h"

namespace PluginFactory {
	uint32_t get_plugin_count(const clap_plugin_factory *factory) { 
		return 1; 
	}

	const clap_plugin_descriptor_t* get_plugin_descriptor(const clap_plugin_factory *factory, uint32_t index) { 
		return index == 0 ? &Plugin::meta : nullptr; 
	}

	const clap_plugin_t* create_plugin(const clap_plugin_factory *factory, const clap_host_t *host, const char *pluginID) {
		if (!clap_version_is_compatible(host->clap_version) || std::string_view(pluginID) != PRODUCT_ID) {
			return nullptr;
		}

        return Plugin::createPlugin(host);
	}

	const clap_plugin_factory_t value = {
		get_plugin_count,
		get_plugin_descriptor,
		create_plugin,
	};
}

namespace EntryPoint {
	bool init(const char* path) {
		return true;
	}
	void deinit() {
	}
	const void* get_factory(const char* factoryId) {
			return std::string_view(CLAP_PLUGIN_FACTORY_ID) == factoryId ? &PluginFactory::value: nullptr;
	}
}

extern "C" const clap_plugin_entry_t clap_entry = {
	CLAP_VERSION_INIT,
	EntryPoint::init,
	EntryPoint::deinit,
	EntryPoint::get_factory,
};

