#include "fileContext.h"

#include <optional>
#include "kitgui/context.h"

namespace {
std::optional<std::string> defaultFileLoader(std::string_view path) {
    // implements file loading using "dumb" c file io
    std::string pathStr(path);
    FILE* file = std::fopen(pathStr.c_str(), "rb");
    if (!file) {
        return std::nullopt;
    }

    // Get file size
    std::fseek(file, 0, SEEK_END);
    long size = std::ftell(file);
    std::fseek(file, 0, SEEK_SET);

    if (size < 0) {
        std::fclose(file);
        return std::nullopt;
    }

    // Read file contents
    std::string contents;
    contents.resize(static_cast<size_t>(size));
    size_t readSize = std::fread(contents.data(), 1, static_cast<size_t>(size), file);
    std::fclose(file);

    if (readSize != static_cast<size_t>(size)) {
        return std::nullopt;
    }

    return contents;
}

}  // namespace

namespace kitgui {

FileContext::FileContext() : mLoader(defaultFileLoader) {}

std::string* FileContext::GetOrLoadFileByName(const std::string& filename, Magnum::InputFileCallbackPolicy policy) {
    // this is a hint from the caller that this file is no longer being used, let's uncache if necessary
    if (policy == Magnum::InputFileCallbackPolicy::Close) {
        auto found = mFiles.find(filename);
        if (found != mFiles.end()) {
            mFiles.erase(found);
        }
        return nullptr;
    }

    // if this file is cached, return that
    auto found = mFiles.find(filename);
    if (found != mFiles.end()) {
        return found->second.get();
    }

    // load file
    std::optional<std::string> fileData = mLoader(filename);
    if (!fileData) {
        return nullptr;
    }

    if (policy == Magnum::InputFileCallbackPolicy::LoadTemporary) {
        // TODO: we should mark this file for later garbage collection
    }
    auto cachedData = std::make_unique<std::string>(std::move(*fileData));
    auto [iter, inserted] = mFiles.emplace(filename, std::move(cachedData));
    return iter->second.get();
}

void FileContext::SetFileLoader(Context::FileLoader fn) {
    mLoader = std::move(fn);
}

}  // namespace kitgui
