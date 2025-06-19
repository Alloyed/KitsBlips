#include <gtest/gtest.h>

#include "kitdsp-gpl/YinPitchDetector.h"
#include "kitdsp/math/util.h"

using namespace kitdsp;

TEST(YinPitchDetector, CanDetectSines) {
    constexpr float frequencyHz = 440.0f;
    constexpr float sampleRate = 48000.0f;
    constexpr size_t bufferSize = 2048;
    // this is about 74 kb
    float workArea[FastYin<bufferSize>::kWorkAreaDesiredSize] = {};
    FastYin<bufferSize> detector(sampleRate, workArea);

    size_t nextSample = 0;
    PitchDetectionResult result;
    for (size_t iteration = 0; iteration < 1; ++iteration) {
        float audioBuffer[bufferSize] = {};
        for (size_t i = 0; i < bufferSize; ++i) {
            audioBuffer[i] = sinf(kTwoPi * nextSample * frequencyHz / sampleRate);
            nextSample++;
        }

        detector.getPitch(audioBuffer, result);
    }

    ASSERT_TRUE(result.pitched);
    ASSERT_NEAR(result.pitch, frequencyHz, 1.0f);
}