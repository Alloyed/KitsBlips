
#include "clapeze/entryPoint.h"

// your program or build system may provide a version of this macro already! if so, use that.
#ifdef _WIN32
#define CLAPEZE_EXPORT __declspec(dllexport)
#else
#define CLAPEZE_EXPORT __attribute__((visibility("default")))
#endif

extern "C" CLAPEZE_EXPORT const clap_plugin_entry_t clap_entry = CLAPEZE_CREATE_ENTRY_POINT();