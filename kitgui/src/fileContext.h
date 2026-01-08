#pragma once

#include <Magnum/FileCallback.h>
#include <memory>
#include <string>
#include <unordered_map>
#include "kitgui/context.h"

namespace kitgui {

class FileContext {
   public:
    explicit FileContext();

    /**
     * Synchronously loads a file into the cache and returns its contents.
     * If the file is already in cache, we'll just return the pointer immediately.
     * The policy parameter helps determine the lifetime of the returned string and associated cache entry.
     */
    std::string* GetOrLoadFileByName(const std::string& filename, Magnum::InputFileCallbackPolicy policy);

    /**
     * Sets the mechanism used to synchronously load files. the default mechanism uses stdio.
     */
    void SetFileLoader(Context::FileLoader mLoader);

   private:
    // std::unique_ptr is used to ensure a stable address for the file data
    std::unordered_map<std::string, std::unique_ptr<std::string>> mFiles;
    Context::FileLoader mLoader;
};

}  // namespace kitgui