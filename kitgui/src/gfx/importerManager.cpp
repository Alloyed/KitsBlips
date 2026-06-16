#include "gfx/importerManager.h"

namespace kitgui {

Corrade::PluginManager::Manager<Magnum::Trade::AbstractImporter>& ImporterManager() {
    static Corrade::PluginManager::Manager<Magnum::Trade::AbstractImporter> sImporterManager{};
    return sImporterManager;
}

}