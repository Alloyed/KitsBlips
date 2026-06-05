#pragma once

#include <cmath>

namespace kitdsp {
/*
 * turns a ratio of inputs (say, eg. a gain factor, or just A/B) into a relative
 * decibel (dbFS) measurement
 */
inline float ratioToDb(float ratio) {
    return 20.0f * std::log10(ratio);
}

/*
 * turns a dbfs measurement into a gain factor
 */
inline float dbToRatio(float db) {
    return std::pow(10.0f, db / 20.0f);
}

/**
 * turns a midi note number (fractional allowed) into a frequency in standard A4=440 western tuning.
 * for ref: midi note 69 is A4, 48 is C3
 */
inline float midiToFrequency(float midiNote, float a4Frequency = 440.0f) {
    return std::exp2((midiNote - 69.0f) / 12.0f) * a4Frequency;
}

/**
 * turns a frequency into a fractional note number in standard A4=440 western tuning.
 * for ref: midi note 69 is A4, 48 is C3
 */
inline float frequencyToMidi(float frequency, float a4Frequency = 440.0f) {
    return 69.0f + 12.0f * std::log2(frequency / a4Frequency);
}

/**
 * turns a number of 12TET semitones (fractional allowed) into a ratio that represents
 * that transposition in the frequency domain. so for example, +12 semitones is
 * 2.0, and -12 semitones is 0.5.
 */
inline float midiToRatio(float semitones) {
    return std::exp2(semitones / 12.0f);
}

inline float msToSamples(float ms, float sampleRate) {
    return ms * sampleRate / 1000.0f;
}

inline float samplesToMs(float samples, float sampleRate) {
    return samples * 1000.0f / sampleRate;
}

}  // namespace kitdsp