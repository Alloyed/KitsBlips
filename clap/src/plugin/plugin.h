#pragma once

#include <clap/clap.h>

namespace Plugin {
	constexpr auto PRODUCT_VENDOR = "Alloyed";
	constexpr auto PRODUCT_URL = "https://github.com/Alloyed/KitsBlips";
	constexpr auto PRODUCT_DESCRIPTION = "https://github.com/Alloyed/KitsBlips";
	constexpr const char *PRODUCT_FEATURES[] = {
		CLAP_PLUGIN_FEATURE_INSTRUMENT,
		CLAP_PLUGIN_FEATURE_SYNTHESIZER,
		CLAP_PLUGIN_FEATURE_STEREO,
		nullptr,
	};
	static const clap_plugin_descriptor_t meta = {
		CLAP_VERSION_INIT,
		PRODUCT_ID,
		PRODUCT_NAME,
		PRODUCT_VENDOR,
		PRODUCT_URL,
		PRODUCT_URL, // manual url
		PRODUCT_URL, // support url
		PRODUCT_VERSION,
		PRODUCT_DESCRIPTION,
		PRODUCT_FEATURES
	};
	const clap_plugin_t* createPlugin(const clap_host_t* host);
}
