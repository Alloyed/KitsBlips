#include "plugin.hpp"
#include <kitdsp/macros.h>

Plugin* pluginInstance;

KITDSP_DLLEXPORT void init(Plugin* p) {
    pluginInstance = p;

    // Add modules here
    p->addModel(modelSnecho);
    p->addModel(modelPSXVerb);
    p->addModel(modelDance);
    p->addModel(modelDsf);

    // Any other plugin initialization may go here.
    // As an alternative, consider lazy-loading assets and lookup tables when
    // your module is created to reduce startup times of Rack.
}
