#pragma once

#include <Magnum/FileCallback.h>
#include <memory>
#include <string>
#include <unordered_map>

namespace kitgui {

class Context;

class FileContext {
   public:
    explicit FileContext(Context& context);

    std::string* getOrLoadFileByName(const std::string& filename, Magnum::InputFileCallbackPolicy policy);

   private:
    Context& mContext;
    // std::unique_ptr is used to ensure a stable address for the file data
    std::unordered_map<std::string, std::unique_ptr<std::string>> mFiles;
};

}  // namespace kitgui