#pragma once

#include <clap/clap.h>
#include <istream>
#include <ostream>
#include <streambuf>
#include "clap/stream.h"

namespace clapeze {
/**
 * Wraps a clap_istream_t into a std::streambuf for use with iostream-based APIs.
 *
 * if your api expects seeking, this won't work! instead use clap_istream_tostring.
 */
class clap_istream_streambuf : public std::streambuf {
    // TODO: seek support. full example at https://stackoverflow.com/a/79746417
   public:
    explicit clap_istream_streambuf(const clap_istream_t* in) : mIn(in) {}

   protected:
    std::streamsize xsgetn(char* s, std::streamsize n) override {
        // read up to n chars from s
        int64_t bytesRead = mIn->read(mIn, s, static_cast<int64_t>(n));
        return (bytesRead <= 0) ? 0 : bytesRead;
    }

    int_type underflow() override {
        // fill internal buffer with more bytes
        int64_t bytesRead = mIn->read(mIn, mBuffer, kBufferSize);
        if (bytesRead <= 0) {
            return traits_type::eof();
        }

        // expose buffer + first char in buffer to stream
        setg(mBuffer, mBuffer, mBuffer + bytesRead);
        return traits_type::to_int_type(mBuffer[0]);
    }

   private:
    const clap_istream_t* mIn;
    static constexpr std::size_t kBufferSize = 256;  // TODO: configurable?
    char mBuffer[kBufferSize];
};

class clap_istream : public std::basic_istream<char> {
    clap_istream_streambuf mBuf;

   public:
    explicit clap_istream(const clap_istream_t* out) : basic_istream(nullptr), mBuf(out) { this->init(&mBuf); }
};

inline std::string clap_istream_tostring(const clap_istream_t* in) {
    clap_istream stream(in);
    return std::string((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
}

/**
 * wraps a clap_ostream_t into a std::streambuf for use with iostream-based APIs.
 */
class clap_ostream_streambuf : public std::streambuf {
   public:
    explicit clap_ostream_streambuf(const clap_ostream_t* out) : mOut(out) {}

   protected:
    std::streamsize xsputn(const char* s, std::streamsize n) override {
        // write s out (size n)
        int64_t bytesWritten = mOut->write(mOut, s, static_cast<int64_t>(n));
        return (bytesWritten <= 0) ? 0 : bytesWritten;
    }

    int_type overflow(int_type c) override {
        // write single char out (c)
        if (c == traits_type::eof()) {
            return traits_type::eof();
        }
        int64_t bytesWritten = mOut->write(mOut, &c, 1);
        if (bytesWritten <= 0) {
            return traits_type::eof();
        }
        return 1;
    }

   private:
    const clap_ostream_t* mOut;
};

class clap_ostream : public std::basic_ostream<char> {
    clap_ostream_streambuf mBuf;

   public:
    explicit clap_ostream(const clap_ostream_t* out) : basic_ostream(nullptr), mBuf(out) { this->init(&mBuf); }
};

}  // namespace clapeze
