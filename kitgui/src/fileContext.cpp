#include "fileContext.h"

#include <fstream>
#include <optional>
#include "kitgui/context.h"

namespace {
std::optional<std::string> defaultFileLoader(std::string_view path) {
    // default file loading using stdio
    constexpr auto read_size = std::size_t(4096);
    auto stream = std::ifstream(std::string(path), std::ios::binary);
    stream.exceptions(std::ios_base::badbit);

    if (not stream) {
        return std::nullopt;
    }

    auto out = std::string();
    auto buf = std::string(read_size, '\0');
    while (stream.read(&buf[0], read_size)) {
        out.append(buf, 0, stream.gcount());
    }
    out.append(buf, 0, stream.gcount());
    return out;
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
