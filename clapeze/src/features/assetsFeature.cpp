#include "clapeze/features/assetsFeature.h"

#include <miniz.h>
#include <miniz_zip.h>
#include <fstream>
#include "clapeze/entryPoint.h"

namespace clapeze {

namespace {
std::string sArchivePath{};
mz_zip_archive sZip{};
int32_t sInstances{};
}  // namespace

AssetsFeature::AssetsFeature() {
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

std::vector<std::string> GetAllPathsFromPlugin() {
    std::vector<std::string> out;
    mz_uint numFiles = mz_zip_reader_get_num_files(&sZip);
    out.reserve(numFiles);

    for (mz_uint idx = 0; idx < numFiles; ++idx) {
        mz_uint bytesNeeded = mz_zip_reader_get_filename(&sZip, idx, nullptr, 0);
        out.emplace_back(bytesNeeded, '\0');
        std::string& buf = out.back();
        mz_uint bytesWritten = mz_zip_reader_get_filename(&sZip, idx, buf.data(), buf.size());
        assert(bytesWritten == bytesNeeded);
    }
    return out;
}

miniz_istream AssetsFeature::OpenFromPlugin(const char* path) {
    return miniz_istream(&sZip, path);
}

std::ifstream AssetsFeature::OpenFromFilesystem(const char* path) {
    return std::ifstream(std::string(path), std::ios::binary);
}

std::ofstream AssetsFeature::OpenFromFilesystemOut(const char* path) {
    return std::ofstream(std::string(path), std::ios::binary);
}

}  // namespace clapeze