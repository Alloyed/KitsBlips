// Loads files from either from disk directly, or from a virtual filesystem embedded within the plugin.
#pragma once

#include <miniz.h>
#include <ios>
#include <istream>
#include "clapeze/features/baseFeature.h"

namespace clapeze {
class PluginHost;

class miniz_istreambuf : public std::streambuf {
    // TODO: seek support. full example at https://stackoverflow.com/a/79746417
   public:
    explicit miniz_istreambuf(mz_zip_archive* pZip, const char* path)
        : mState(mz_zip_reader_extract_file_iter_new(pZip, path, kLoadFlags)) {}
    ~miniz_istreambuf() { mz_zip_reader_extract_iter_free(mState); }
    miniz_istreambuf(const miniz_istreambuf&) = delete;
    miniz_istreambuf(miniz_istreambuf&&) = delete;
    miniz_istreambuf& operator=(const miniz_istreambuf&) = delete;
    miniz_istreambuf& operator=(miniz_istreambuf&&) = delete;

   protected:
    static constexpr mz_uint kLoadFlags = 0;
    std::streamsize xsgetn(char* s, std::streamsize n) override {
        // read up to n chars from s
        size_t bytesRead = mz_zip_reader_extract_iter_read(mState, s, n);
        return static_cast<std::streamsize>((bytesRead <= 0) ? 0 : bytesRead);
    }

    int_type underflow() override {
        // fill internal buffer with more bytes
        size_t bytesRead = mz_zip_reader_extract_iter_read(mState, mBuffer, kBufferSize);
        if (bytesRead <= 0) {
            return traits_type::eof();
        }

        // expose buffer + first char in buffer to stream
        setg(mBuffer, mBuffer, mBuffer + bytesRead);
        return traits_type::to_int_type(mBuffer[0]);
    }

   private:
    mz_zip_reader_extract_iter_state* mState;
    static constexpr std::size_t kBufferSize = 256;  // TODO: configurable?
    char mBuffer[kBufferSize];
};

/**
 * Wraps a miniz file into a std::streambuf for use with iostream-based APIs.
 *
 * if your api expects seeking, this won't work! instead use istream_tostring.
 */
class miniz_istream : public std::basic_istream<char> {
   public:
    explicit miniz_istream(mz_zip_archive* pZip, const char* path) : basic_istream(nullptr), mBuf(pZip, path) {
        this->init(&mBuf);
    }

   private:
    miniz_istreambuf mBuf;
};

class AssetsFeature : public BaseFeature {
   public:
    explicit AssetsFeature();
    ~AssetsFeature() override;
    AssetsFeature(const AssetsFeature&) = delete;
    AssetsFeature(AssetsFeature&&) = delete;
    AssetsFeature& operator=(const AssetsFeature&) = delete;
    AssetsFeature& operator=(AssetsFeature&&) = delete;

    static constexpr auto NAME = "clapeze.assets";
    const char* Name() const override { return NAME; }

    void Configure(BasePlugin& self) override;

    miniz_istream OpenFromPlugin(const char* path);
    std::ifstream OpenFromFilesystem(const char* path);
    std::ofstream OpenFromFilesystemOut(const char* path);
};
}  // namespace clapeze