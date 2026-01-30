#pragma once

#include "kitdsp/control/approach.h"

// a gate is a simple envelope that be used for short on-off events in place of an ADSR or similar

namespace kitdsp {
class Gate {
   public:
    Gate() {}
    ~Gate() = default;
    inline void Reset() { mApproach.Reset(); }
    void SetSampleRate(float sampleRate) { mApproach.SetHalfLife(1.0f, sampleRate); }
    void TriggerOpen() { mApproach.target = 1.0f; }
    void TriggerClose() { mApproach.target = 0.0f; }
    void TriggerChoke() { mApproach.target = 0.0f; }
    float Process() { return mApproach.Process(); }
    float GetValue() const { return mApproach.current; }
    bool IsProcessing() const { return mApproach.target != 0.0f || mApproach.IsChanging(); }

   private:
    Approach mApproach{};
};
}  // namespace kitdsp