#ifndef GGD_WAV_READER_H
#define GGD_WAV_READER_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    float    *samples;
    uint32_t  num_frames;
    uint32_t  sample_rate;
    uint16_t  num_channels;
} WavFile;

bool wav_read(const char *path, WavFile *out);
void wav_free(WavFile *wav);

#endif
