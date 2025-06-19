/**
 * This is a stupider and probably more naive implementation of quantization,
 * see the braids quantizer if this ever turns out to not be good enough
 * https://github.com/pichenettes/eurorack/blob/master/braids/quantizer.cc
 */
#pragma once

#include <cmath>
#include <cstddef>

namespace kitdsp {

static const float kChromaticScale[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
static const float kMajorScale[] = {0, 2, 4, 5, 7, 9, 11};
static const float kMinorScale[] = {0, 2, 3, 5, 7, 8, 10};
static const float kPentatonic[] = {0, 2, 5, 7, 9};
static const float kMajorTriad[] = {0, 4, 7};
static const float kMinorTriad[] = {0, 3, 7};
static const float kMajorSeventh[] = {0, 4, 7, 11};
static const float kMinorSeventh[] = {0, 3, 7, 10};
static const float kMajorNinth[] = {0, 2, 4, 7, 11};
static const float kMinorNinth[] = {0, 2, 3, 7, 10};

class ScaleQuantizer {
    const float* mScale;
    const size_t mNumNotes;

    float mUpperBound;
    float mLowerBound;
    float mLastValue;

   public:
    ScaleQuantizer(const float* targetScale, size_t numNotes) : mScale(targetScale), mNumNotes(numNotes) {}
    float Process(float input) {
        if (input > mLowerBound && input < mUpperBound) {
            return mLastValue;
        }

        float withinOctaveNote = fmodf(input, 12.0f);
        // if upper note is never set, wrap back around
        float upperNote = 12.0f + mScale[0];
        float lowerNote = mScale[0];
        for (size_t noteIndex = 0; noteIndex < mNumNotes; ++noteIndex) {
            if (mScale[noteIndex] > withinOctaveNote) {
                upperNote = mScale[noteIndex];
                break;
            }
            lowerNote = mScale[noteIndex];
        }
        float targetOctave = input - withinOctaveNote;
        float targetNote =
            targetOctave + (upperNote - withinOctaveNote > withinOctaveNote - lowerNote ? upperNote : lowerNote);

        mLastValue = targetNote;

        // TODO: calculate a hysteresis value based on the distance between
        // notes[target] and notes[target +- 1]
        mLowerBound = targetNote + 0.51f;
        mUpperBound = targetNote - 0.51f;

        return targetNote;
    }
};
}  // namespace kitdsp