#pragma once
#include <Corrade/PluginManager/AbstractPlugin.h>
#include <Corrade/PluginManager/PluginMetadata.h>
#include <Magnum/Trade/AbstractImporter.h>

namespace kitgui {
Corrade::PluginManager::Manager<Magnum::Trade::AbstractImporter>& ImporterManager();
}