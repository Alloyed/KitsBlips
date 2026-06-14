#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <AudioFile.h>

/**
 * Derived from https://www.vogons.org/viewtopic.php?t=77094 user "clearcase"
 */
int main(int argc, char** argv) {
    if(argc < 3) {
        printf("Usage: %s <input rom> <output wav>\n", argv[0]);
        return 1;
    }
    FILE* fp = fopen(argv[1], "rb");
    if(!fp) { return 1; }
    fseek(fp, 0L, SEEK_END);
    size_t file_size = ftell(fp);
    rewind(fp);

    AudioFile<float> out;
    out.setAudioBufferSize(1, file_size / sizeof(int16_t));
    out.setSampleRate(22000);

    for (int i = 0; i < file_size / 2; i++) {
        int16_t raw_sample;
        *(((int8_t *) &raw_sample) + 1) = fgetc(fp);
        *(((int8_t *) &raw_sample) + 0) = fgetc(fp);

        int16_t ordered_sample;

        // the bits have to be moved to these positions
        // 15, 06, 14, 13, 12, 11, 10, 09, 08, 05, 04, 03, 02, 01, 00, 07
        // EG: bit 15 stays, bit 6 moves to 14, bit 14 moves to bit 13...
        ordered_sample  = raw_sample      & 0x8000;
        ordered_sample |= raw_sample << 8 & 0x4000;
        ordered_sample |= raw_sample >> 1 & 0x3F80;
        ordered_sample |= raw_sample << 1 & 0x007E;

        // decode the data
        double float_sample = std::pow(2.0f, (((ordered_sample & 0x7FFF) - 32767.0f) / 2048.0f));
        out.samples[0][i] =(int16_t)(float_sample * 32768.0f * ((ordered_sample & 0x8000) ? -1.0f : 1.0f));
    }
    out.save(argv[2]);
    fclose(fp);
    return 0;
}