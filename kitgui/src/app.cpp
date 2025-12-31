#include "kitgui/app.h"

#include <cstdio>
#include <optional>
#include <string>
#include <string_view>

namespace kitgui {

std::optional<std::string> BaseApp::OnFileLoadRequest(std::string_view path) {
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

}  // namespace kitgui


