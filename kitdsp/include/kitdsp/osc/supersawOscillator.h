
namespace kitdsp {
class SuperSawOscillator : public Phasor {
   public:
    float Process() {
        Advance();
        return mPhase * 2.0f - 1.0f;
    }
};
}

#if 0
/*
 * check out
 * https://youtu.be/XM_q5T7wTpQ?t=1741
 * https://www.adamszabo.com/internet/adam_szabo_how_to_emulate_the_super_saw.pdf
 * important detail this code doesn't capture: this should run 88.2KHz, and the high pass filter should be placed just below the fundamental (to reduce aliasing noise)
 */
int24_t saw[7] = {0};

const int24_t detune_table[7] = { 0, 128, - 128, 816, -824, 1408, 1440 };

int24_t next(int24_t pitch, int24_t spread, int24_t detune) {
    int24_t sum = 0 ;

    for (int i = 0; i < 7; i++= {
        int24_t voice_detune = (detune_table[i] * (pitch * detune)) >> 7;
        saw[i] += pitch + voice_detune;

        if ( i == 0)
            sum += saw[i];
        else
        sum += saw[i] * spread;
    }

    return high_pass(sum);
}
#endif
