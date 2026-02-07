#pragma once
#include <cstddef>

namespace kitdsp {
/**
 * Adapted from
 * https://www.musicdsp.org/en/latest/Filters/39-polyphase-filters.html
 * This is a polyphase IIR-filter made by cascading all-pass filters on top of each other.
 * Because the target frequencies are fixed we hardcode the coefficients involved
 * Also, we don't need sample rate! because the frequency is a ratio of sample rate and all
 */
template <size_t STAGES>
class HalfbandFilter {
   public:
    HalfbandFilter() {
        static_assert(STAGES <= 4);
        static_assert(STAGES != 0);

        if (STAGES == 4) {
            // rejection=69dB, transition band=0.01
            mFilterA[0].coef = 0.07711507983241622f;
            mFilterA[1].coef = 0.48207062506104720f;
            mFilterA[2].coef = 0.79682047133157970f;
            mFilterA[3].coef = 0.94125142777404710f;
            mFilterB[0].coef = 0.26596852652109460f;
            mFilterB[1].coef = 0.66510415326349570f;
            mFilterB[2].coef = 0.88410150855061590f;
            mFilterB[3].coef = 0.98200541418860750f;
        } else if (STAGES == 3) {
            // rejection=51dB, transition band=0.01
            mFilterA[0].coef = 0.12714141362648530f;
            mFilterA[1].coef = 0.65282458863691170f;
            mFilterA[2].coef = 0.91769428343281150f;
            mFilterB[0].coef = 0.40056789819445626f;
            mFilterB[1].coef = 0.82041638919233430f;
            mFilterB[2].coef = 0.97631145158367730f;
        } else if (STAGES == 2) {
            // rejection=53dB, transition band=0.05
            mFilterA[0].coef = 0.12073211751675449f;
            mFilterA[1].coef = 0.66320202241939950f;
            mFilterB[0].coef = 0.39036218723450060f;
            mFilterB[1].coef = 0.89078683265349700f;
        } else if (STAGES == 1) {
            // rejection=36dB, transition band=0.1
            mFilterA[0].coef = 0.23647102099689224f;
            mFilterB[0].coef = 0.71454214971260010f;
        }
    }

    float Process(float in) {
        float filterA = in;
        for (size_t i = 0; i < STAGES; ++i) {
            filterA = mFilterA[i].Process(filterA);
        }

        float filterB = mLastIn;
        for (size_t i = 0; i < STAGES; ++i) {
            filterB = mFilterB[i].Process(filterB);
        }

        mLastIn = in;
        return filterA + filterB;
    }

    void Reset() {
        for (size_t i = 0; i < STAGES; ++i) {
            mFilterA[i].Reset();
            mFilterB[i].Reset();
        }
        mLastIn = 0.0f;
    }

   private:
    struct AllPassStage {
        float Process(float in) {
            inputs[2] = inputs[1];
            inputs[1] = inputs[0];
            inputs[0] = in;

            outputs[2] = outputs[1];
            outputs[1] = outputs[0];
            outputs[0] = inputs[2] + coef * (in - outputs[2]);

            return outputs[0];
        }

        void Reset() {
            for (int i = 0; i < 3; ++i) {
                inputs[i] = 0.0f;
                outputs[i] = 0.0f;
            }
        }

        float coef{};
        float inputs[3]{};
        float outputs[3]{};
    };

    AllPassStage mFilterA[STAGES]{};
    AllPassStage mFilterB[STAGES]{};
    float mLastIn{};
};
}  // namespace kitdsp