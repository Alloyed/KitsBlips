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

    std::string* GetOrLoadFileByName(const std::string& filename, Magnum::InputFileCallbackPolicy policy);
    void SetFileLoader(Context::FileLoader mLoader);

   private:
    // std::unique_ptr is used to ensure a stable address for the file data
    std::unordered_map<std::string, std::unique_ptr<std::string>> mFiles;
    Context::FileLoader mLoader;
};

}  // namespace kitgui