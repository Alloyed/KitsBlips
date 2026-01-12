#include "clapeze/ext/assets.h"

#include <fmt/format.h>
#include <miniz.h>
#include <miniz_zip.h>
#include <fstream>
#include "clapeze/entryPoint.h"
#include "clapeze/pluginHost.h"

namespace clapeze {

namespace {
std::string sArchivePath{};
mz_zip_archive sZip{};
int32_t sInstances{};
}  // namespace

AssetsFeature::AssetsFeature(PluginHost& host) : mHost(host) {
    sInstances++;
    if (sInstances == 1) {
        sArchivePath = getPluginPath();
        mz_zip_zero_struct(&sZip);
        mz_zip_reader_init_file(&sZip, sArchivePath.c_str(), 0);
    }
}
AssetsFeature::~AssetsFeature() {
    sInstances--;
    if (sInstances == 0) {
        mz_zip_reader_end(&sZip);
    }
}

void AssetsFeature::Configure(BasePlugin& self) {}

bool AssetsFeature::LoadFromPlugin(const char* path, std::string& out) {
    out.clear();
    size_t uncomp_size{};
    char* p = nullptr;
    // Try to extract all the files to the heap.
    p = static_cast<char*>(mz_zip_reader_extract_file_to_heap(&sZip, path, &uncomp_size, 0));
    if (!p) {
        auto* errorText = mz_zip_get_error_string(mz_zip_get_last_error(&sZip));
        mHost.Log(LogSeverity::Error, fmt::format("reading {} from archive failed: {}", path, errorText));
        return false;
    }
    out.assign(p, p + uncomp_size);
    mz_free(p);
    return true;
}
bool AssetsFeature::LoadFromFilesystem(const char* path, std::string& out) {
    out.clear();
    constexpr auto read_size = std::size_t(4096);
    auto stream = std::ifstream(std::string(path));
    stream.exceptions(std::ios_base::badbit);

    if (!stream) {
        mHost.Log(LogSeverity::Error, fmt::format("reading {} from filesystem failed", path));
        return false;
    }

    auto buf = std::string(read_size, '\0');
    while (stream.read(&buf[0], read_size)) {
        out.append(buf, 0, stream.gcount());
    }
    out.append(buf, 0, stream.gcount());
    return true;
}
}  // namespace clapeze