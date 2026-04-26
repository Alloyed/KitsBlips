
#include "kitdsp/control/lfo.h"
#include "kitdsp/delayLine.h"
#include "kitdsp/math/approx.h"
#include "kitdsp/math/interpolate.h"
#include "kitdsp/math/util.h"

namespace {
float hanningWindow(float t) {
    if (t <= 0.0f || t >= 1.0f) {
        return 0.0f;
    }
    return 0.5f * (1.0f - kitdsp::approx::cos2pif_nasty(t));
}

float rectWindow(float t) {
    if (t <= 0.0f || t >= 1.0f) {
        return 0.0f;
    }
    return 1.0f;
}
}  // namespace

namespace kitdsp {
namespace pitch {
/**
 * PSOLA stands for (P)itch-(S)ynchronous (O)ver(L)ap-and-(A)dd
 * It works by splitting the input signal into grains that are a clean multiple
 * of the original fundamental pitch (ie. pitch-synchronous), then playing them back. The grains should be overlapping
 * with a hann-window to avoid hearing the seams.
 *  - If you repeat a grain, then you get higher duration with the same pitch.
 *  - If you play the grain back faster, you get lower duration with a higher pitch.
 * You can combine the two to get a different pitch, and have the duration changes cancel out.
 */
class PsolaPitchShifter {
    struct Grain {
        float sizeSamples = 0.0f;
        float samplesPlayed = 0.0f;
        float speed = 0.0f;
        explicit Grain(DelayLine<float>& buf) : mBuf(buf) {}
        void Set(float start, float size, float speed) {
            this->pos = start;
            this->sizeSamples = size;
            this->speed = speed;
            this->samplesPlayed = 0.0f;
        }
        void Advance() {
            pos = std::max(0.0f, pos + 1.0f - speed);
            samplesPlayed += speed;
        }
        float Read() {
            if (sizeSamples == 0.0f) {
                return 0.0f;
            }
            float progress = kitdsp::clamp(samplesPlayed / sizeSamples, 0.0f, 1.0f);
            using namespace kitdsp::interpolate;
            return mBuf.Read<InterpolationStrategy::Linear>(pos) * hanningWindow(progress);
        }

       private:
        float pos;
        DelayLine<float>& mBuf;
    };

   public:
    explicit PsolaPitchShifter(etl::span<float> buf) : mBuf(buf), mGrain1(mBuf), mGrain2(mBuf) { Reset(); }
    void Reset() { mBuf.Reset(); }
    void SetParams(float pitchMultiplier, float sampleRate) {
        mSpeed = pitchMultiplier;
        mPeriod = 0.05f * sampleRate;
        float overlapRatio = 1.0f - 0.2f;  // TODO not this
        assert(pitchMultiplier != 0);
        mGrainPicker.SetPeriodSamples(overlapRatio * (mPeriod / pitchMultiplier));
    }
    float Process(float input) {
        mBuf.Write(input);
        mGrain1.Advance();
        mGrain2.Advance();

        if (mGrainPicker.Process()) {
            // TODO: use autocorrelation to pick high quality next grain: this is more like minimum latency last grain
            if (nextGrain == 1) {
                mGrain1.Set(mPeriod, mPeriod, mSpeed);
                nextGrain = 2;
            } else {
                mGrain2.Set(mPeriod, mPeriod, mSpeed);
                nextGrain = 1;
            }
        }

        return mGrain1.Read() + mGrain2.Read();
    }

   private:
    DelayLine<float> mBuf;
    lfo::ImpulseTrain mGrainPicker;
    Grain mGrain1;
    Grain mGrain2;
    size_t nextGrain = 1;
    float mSpeed = 1.0f;
    float mPeriod = 0.1f;
};
}  // namespace pitch
}  // namespace kitdsp