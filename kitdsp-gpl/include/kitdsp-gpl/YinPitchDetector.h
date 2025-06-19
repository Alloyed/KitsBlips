#pragma once

#include <cstring>
#include "kitdsp/math/shy_fft.h"

/*
 * C++ adaptation of the TarsosDSP "FastYin" algorithm. changes:
 * - use shy_fft as the fft implementation
 * - avoid internal allocation
 * - float threshold instead of doubles
 *
 * for easier diffing existing name standards are _not_ applied to this file.
 *
 * @KitDSP
 * ---
 *  TarsosDSP is developed by Joren Six at
 *  The Royal Academy of Fine Arts & Royal Conservatory,
 *  University College Ghent,
 *  Hoogpoort 64, 9000 Ghent - Belgium
 *
 *  Info: http://0110.be/tag/TarsosDSP
 *  Github: https://github.com/JorenSix/TarsosDSP
 *  Releases: http://0110.be/releases/TarsosDSP/
 *
 */

namespace kitdsp {
/**
 * A class with information about the result of a pitch detection on a block of
 * audio.
 *
 * It contains:
 *
 * <ul>
 * <li>The pitch in Hertz.</li>
 * <li>A probability (noisiness, (a)periodicity, salience, voicedness or clarity
 * measure) for the detected pitch. This is somewhat similar to the term voiced
 * which is used in speech recognition. This probability should be calculated
 * together with the pitch. The exact meaning of the value depends on the detector used.</li>
 * <li>A way to calculate the RMS of the signal.</li>
 * <li>A boolean that indicates if the algorithm thinks the signal is pitched or
 * not.</li>
 * </ul>
 *
 * The separate pitched or unpitched boolean can coexist with a defined pitch.
 * E.g. if the algorithm detects 220Hz in a noisy signal it may respond with
 * 220Hz "unpitched".
 *
 * <p>
 * For performance reasons the object is reused. Please create a copy of the object
 * if you want to use it on an other thread.
 *
 *
 * @author Joren Six
 */
struct PitchDetectionResult {
    /**
     * The pitch in Hertz.
     */
    float pitch = -1;

    /**
     * @return A probability (noisiness, (a)periodicity, salience, voicedness or
     *         clarity measure) for the detected pitch. This is somewhat similar
     *         to the term voiced which is used in speech recognition. This
     *         probability should be calculated together with the pitch. The
     *         exact meaning of the value depends on the detector used.
     */
    float probability = -1;

    /**
     * @return Whether the algorithm thinks the block of audio is pitched. Keep
     *         in mind that an algorithm can come up with a best guess for a
     *         pitch even when isPitched() is false.
     */
    bool pitched = false;
};

/**
 * An implementation of the YIN pitch tracking algorithm which uses an FFT to
 * calculate the difference function. This makes calculating the difference
 * function more performant. See <a href=
 * "http://recherche.ircam.fr/equipes/pcm/cheveign/ps/2002_JASA_YIN_proof.pdf"
 * >the YIN paper.</a> This implementation is done by <a href="mailto:matthias.mauch@elec.qmul.ac.uk">Matthias Mauch</a>
 * and is based on {@link Yin} which is based on the implementation found in <a href="http://aubio.org">aubio</a> by
 * Paul Brossier.
 *
 * @author Matthias Mauch
 * @author Joren Six
 * @author Paul Brossier
 */
template <size_t bufferSize = 2048>
class FastYin {
   public:
    /**
     * when being used on a recording longer than the buffer size, how many samples is it suggested that you re-use from
     * the last call to getPitch()? The default overlap of two consecutive audio buffers (in samples).
     */
    static constexpr size_t kSuggestedBufferOverlapSize = bufferSize * 3 / 4;

    // this is 9 * bufferSize. this can probably be optimized further.
    static constexpr size_t kWorkAreaDesiredSize =
        /* yinBuffer */ (bufferSize / 2) +
        /* powerTerms */ (bufferSize / 2) +
        /* audioBufferFFT */ (2 * bufferSize) +
        /* fftInput */ (2 * bufferSize) +
        /* kernel */ (2 * bufferSize) +
        /* yinStyleACF */ (2 * bufferSize);

    /**
     * The default YIN threshold value. Should be around 0.10~0.15. See YIN
     * paper for more information.
     */
    static constexpr float kDefaultThreshold = 0.20f;

    /**
     * Create a new pitch detector for a stream with the defined sample rate.
     * Processes the audio in blocks of the defined size.
     *
     * @param audioSampleRate
     *            The sample rate of the audio stream. E.g. 44.1 kHz.
     * @param workArea
     *            chunk of memory used for internal computation. must be kWorkAreaDesiredSize floats big.
     * @param yinThreshold
     *            The parameter that defines which peaks are kept as possible
     *            pitch candidates. See the YIN paper for more details.
     */
    FastYin(float audioSampleRate, float* workArea, float yinThreshold = kDefaultThreshold)
        : sampleRate(audioSampleRate), threshold(yinThreshold), audioBuffer_size(bufferSize) {
        float* p = workArea;

        yinBuffer = p;
        yinBuffer_size = bufferSize / 2;
        p += yinBuffer_size;

        powerTerms = p;
        powerTerms_size = bufferSize / 2;
        p += powerTerms_size;

        audioBufferFFT = p;
        audioBufferFFT_size = 2 * bufferSize;
        p += audioBufferFFT_size;

        fftInput = p;
        fftInput_size = 2 * bufferSize;
        p += fftInput_size;

        kernel = p;
        kernel_size = 2 * bufferSize;
        p += kernel_size;

        yinStyleACF = p;
        yinStyleACF_size = 2 * bufferSize;
        p += yinStyleACF_size;

        fft.Init();
    }

    /**
     * The main flow of the YIN algorithm. Returns a pitch value in Hz or -1 if
     * no pitch is detected.
     *
     * @return a pitch value in Hz or -1 if no pitch is detected.
     */
    void getPitch(const float* audioBuffer, PitchDetectionResult& result) {
        int tauEstimate;
        float pitchInHertz;

        // step 2
        difference(audioBuffer);

        // step 3
        cumulativeMeanNormalizedDifference();

        // step 4
        tauEstimate = absoluteThreshold(result);

        // step 5
        if (tauEstimate != -1) {
            float betterTau = parabolicInterpolation(tauEstimate);

            // step 6
            // TODO Implement optimization for the AUBIO_YIN algorithm.
            // 0.77% => 0.5% error rate,
            // using the data of the YIN paper
            // bestLocalEstimate()

            // conversion to Hz
            pitchInHertz = sampleRate / betterTau;
        } else {
            // no pitch found
            pitchInHertz = -1;
        }

        result.pitch = (pitchInHertz);
    }

   private:
    /**
     * The actual YIN threshold.
     */
    float threshold;

    /**
     * The audio sample rate. Most audio has a sample rate of 44.1kHz.
     */
    float sampleRate;

    size_t audioBuffer_size;

    /**
     * The buffer that stores the calculated values. It is exactly half the size
     * of the input buffer.
     */
    float* yinBuffer;
    size_t yinBuffer_size;

    /**
     * kitdsp: pulled out of difference() function. same size as yinBuffer
     */
    float* powerTerms;
    size_t powerTerms_size;

    //------------------------ FFT instance members

    /**
     * Holds the FFT data, twice the length of the audio buffer.
     */
    float* audioBufferFFT;
    size_t audioBufferFFT_size;

    /**
     * kitdsp: used as input/work buffer for fft algorithm
     */
    float* fftInput;
    size_t fftInput_size;

    /**
     * Half of the data, disguised as a convolution kernel.
     */
    float* kernel;
    size_t kernel_size;

    /**
     * Buffer to allow convolution via complex multiplication. It calculates the auto correlation function (ACF).
     */
    float* yinStyleACF;
    size_t yinStyleACF_size;

    /**
     * An FFT object to quickly calculate the difference function.
     */
    kitdsp::ShyFFT<float, bufferSize> fft;

    /**
     * Implements the difference function as described in step 2 of the YIN
     * paper with an FFT to reduce the number of operations.
     */
    void difference(const float* audioBuffer) {
        // POWER TERM CALCULATION
        // ... for the power terms in equation (7) in the Yin paper
        powerTerms[0] = 0.0f;
        for (int j = 0; j < yinBuffer_size; ++j) {
            powerTerms[0] += audioBuffer[j] * audioBuffer[j];
        }
        // now iteratively calculate all others (saves a few multiplications)
        for (int tau = 1; tau < yinBuffer_size; ++tau) {
            powerTerms[tau] = powerTerms[tau - 1] - audioBuffer[tau - 1] * audioBuffer[tau - 1] +
                              audioBuffer[tau + yinBuffer_size] * audioBuffer[tau + yinBuffer_size];
        }

        // YIN-STYLE AUTOCORRELATION via FFT
        // 1. data
        memset(fftInput, 0, fftInput_size * sizeof(float));
        for (int j = 0; j < audioBuffer_size; ++j) {
            fftInput[real(j)] = audioBuffer[j];
        }
        fft.Direct(fftInput, audioBufferFFT);

        // 2. half of the data, disguised as a convolution kernel
        memset(fftInput, 0, fftInput_size * sizeof(float));
        for (int j = 0; j < yinBuffer_size; ++j) {
            fftInput[real(j, kernel_size)] = audioBuffer[(yinBuffer_size - 1) - j];
        }
        fft.Inverse(fftInput, kernel);

        // 3. convolution via complex multiplication
        for (int j = 0; j < audioBuffer_size; ++j) {
            fftInput[real(j)] =
                audioBufferFFT[real(j)] * kernel[real(j)] - audioBufferFFT[imag(j)] * kernel[imag(j)];  // real
        }
        for (int j = 0; j < audioBuffer_size; ++j) {
            fftInput[imag(j)] =
                audioBufferFFT[imag(j)] * kernel[real(j)] + audioBufferFFT[real(j)] * kernel[imag(j)];  // imaginary
        }
        fft.Inverse(fftInput, yinStyleACF);
        kitdsp_scaleFFT(yinStyleACF, yinStyleACF_size, true);

        // CALCULATION OF difference function
        // ... according to (7) in the Yin paper.
        for (int j = 0; j < yinBuffer_size; ++j) {
            // taking only the real part
            yinBuffer[j] = powerTerms[0] + powerTerms[j] - 2 * yinStyleACF[real(yinBuffer_size - 1 + j)];
        }
    }

    /**
     * The cumulative mean normalized difference function as described in step 3
     * of the YIN paper. <br>
     * <code>
     * yinBuffer[0] == yinBuffer[1] = 1
     * </code>
     */
    void cumulativeMeanNormalizedDifference() {
        int tau;
        yinBuffer[0] = 1;
        float runningSum = 0;
        for (tau = 1; tau < yinBuffer_size; tau++) {
            runningSum += yinBuffer[tau];
            yinBuffer[tau] *= tau / runningSum;
        }
    }

    /**
     * Implements step 4 of the AUBIO_YIN paper.
     */
    int absoluteThreshold(PitchDetectionResult& result) {
        // Uses another loop construct
        // than the AUBIO implementation
        int tau;
        // first two positions in yinBuffer are always 1
        // So start at the third (index 2)
        for (tau = 2; tau < yinBuffer_size; tau++) {
            if (yinBuffer[tau] < threshold) {
                while (tau + 1 < yinBuffer_size && yinBuffer[tau + 1] < yinBuffer[tau]) {
                    tau++;
                }
                // found tau, exit loop and return
                // store the probability
                // From the YIN paper: The threshold determines the list of
                // candidates admitted to the set, and can be interpreted as the
                // proportion of aperiodic power tolerated
                // within a periodic signal.
                //
                // Since we want the periodicity and and not aperiodicity:
                // periodicity = 1 - aperiodicity
                result.probability = (1 - yinBuffer[tau]);
                break;
            }
        }

        // if no pitch found, tau => -1
        if (tau == yinBuffer_size || yinBuffer[tau] >= threshold || result.probability > 1.0) {
            tau = -1;
            result.probability = (0);
            result.pitched = (false);
        } else {
            result.pitched = (true);
        }

        return tau;
    }

    /**
     * Implements step 5 of the AUBIO_YIN paper. It refines the estimated tau
     * value using parabolic interpolation. This is needed to detect higher
     * frequencies more precisely. See http://fizyka.umk.pl/nrbook/c10-2.pdf and
     * for more background
     * http://fedc.wiwi.hu-berlin.de/xplore/tutorials/xegbohtmlnode62.html
     *
     * @param tauEstimate
     *            The estimated tau value.
     * @return A better, more precise tau value.
     */
    float parabolicInterpolation(int tauEstimate) {
        float betterTau;
        int x0;
        int x2;

        if (tauEstimate < 1) {
            x0 = tauEstimate;
        } else {
            x0 = tauEstimate - 1;
        }
        if (tauEstimate + 1 < yinBuffer_size) {
            x2 = tauEstimate + 1;
        } else {
            x2 = tauEstimate;
        }
        if (x0 == tauEstimate) {
            if (yinBuffer[tauEstimate] <= yinBuffer[x2]) {
                betterTau = tauEstimate;
            } else {
                betterTau = x2;
            }
        } else if (x2 == tauEstimate) {
            if (yinBuffer[tauEstimate] <= yinBuffer[x0]) {
                betterTau = tauEstimate;
            } else {
                betterTau = x0;
            }
        } else {
            float s0, s1, s2;
            s0 = yinBuffer[x0];
            s1 = yinBuffer[tauEstimate];
            s2 = yinBuffer[x2];
            // fixed AUBIO implementation, thanks to Karl Helgason:
            // (2.0f * s1 - s2 - s0) was incorrectly multiplied with -1
            betterTau = tauEstimate + (s2 - s0) / (2 * (2 * s1 - s2 - s0));
        }
        return betterTau;
    }

    // kitdsp-specific methods
    // normalization as implemented in FloatFFT.java
    void kitdsp_scaleFFT(float* a, size_t a_size, bool complex) {
        int m = a_size / 2;
        int n = m;
        float norm = (float)(1.0 / m);
        int n2;
        if (complex) {
            n2 = 2 * n;
        } else {
            n2 = n;
        }
        for (int i = 0; i < n2; i++) {
            a[i] *= norm;
        }
    }
    // shyfft is non-interleaved, TarsosDSP is interleaved. this helps hide the difference
    size_t real(size_t index, size_t size = 0) { return index; }

    // shyfft is non-interleaved, TarsosDSP is interleaved. this helps hide the difference
    size_t imag(size_t index, size_t size = SIZE_MAX) {
        size = size == SIZE_MAX ? audioBuffer_size : size;
        return size + index;
    }
};
}  // namespace kitdsp