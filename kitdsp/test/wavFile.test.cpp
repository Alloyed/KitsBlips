#include "kitdsp/wavFile.h"
#include <gtest/gtest.h>

using namespace kitdsp;

TEST(chorus, works) {
    constexpr float sampleRate = 44100;

    FILE* fpin = fopen("../guitar.wav", "rb");
    ASSERT_NE(fpin, nullptr);

    FILE* fpout = fopen("wavout.wav", "wb");
    ASSERT_NE(fpout, nullptr);

    WavFileReader<2> fin{sampleRate, fpin};
    WavFileWriter<1> fout{sampleRate, fpout};

    fout.Start();
    fout.Finish();

    fclose(fpin);
    fclose(fpout);
}
