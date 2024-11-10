/**
 * This is a stupider and probably more naive implementation of quantization, see the braids quantizer if this ever turns out to not be good enough
 * https://github.com/pichenettes/eurorack/blob/master/braids/quantizer.cc
 */
#pragma once

#include <cstddef>
#include <cmath>

static const float kChromaticScale[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
static const float kMajorScale[]     = {0, 2, 4, 5, 7, 9, 11};
static const float kMinorScale[]     = {0, 2, 3, 5, 7, 8, 10};
static const float kPentatonic[]     = {0, 2, 5, 7, 9};
static const float kMajorTriad[]     = {0, 4, 7};
static const float kMinorTriad[]     = {0, 3, 7};
static const float kMajorSeventh[]   = {0, 4, 7, 11};
static const float kMinorSeventh[]   = {0, 3, 7, 10};
static const float kMajorNinth[]     = {0, 2, 4, 7, 11};
static const float kMinorNinth[]     = {0, 2, 3, 7, 10};

class ScaleQuantizer
{
    const float* m_scale;
    const size_t m_numNotes;

    float m_upperBound;
    float m_lowerBound;
    float m_lastValue;

  public:
    ScaleQuantizer(const float* targetScale, size_t numNotes)
    : m_scale(targetScale), m_numNotes(numNotes)
    {
    }
    float Process(float input)
    {
        if(input > m_lowerBound && input < m_upperBound)
        {
            return m_lastValue;
        }

        float withinOctaveNote = fmodf(input, 12.0f);
        // if upper note is never set, wrap back around
        float upperNote = 12.0f + m_scale[0];
        float lowerNote = m_scale[0];
        for(size_t noteIndex = 0; noteIndex < m_numNotes; ++noteIndex)
        {
            if(m_scale[noteIndex] > withinOctaveNote)
            {
                upperNote = m_scale[noteIndex];
                break;
            }
            lowerNote = m_scale[noteIndex];
        }
        float targetOctave = input - withinOctaveNote;
        float targetNote
            = targetOctave
              + (upperNote - withinOctaveNote > withinOctaveNote - lowerNote
                     ? upperNote
                     : lowerNote);

        m_lastValue = targetNote;

        // TODO: calculate a hysteresis value based on the distance between notes[target] and notes[target +- 1]
        m_lowerBound = targetNote + 0.51f;
        m_upperBound = targetNote - 0.51f;

        return targetNote;
    }
};