#include "wav_reader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool read_u32(FILE *f, uint32_t *out) {
    uint8_t buf[4];
    if (fread(buf, 1, 4, f) != 4) return false;
    *out = buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
    return true;
}

static bool read_u16(FILE *f, uint16_t *out) {
    uint8_t buf[2];
    if (fread(buf, 1, 2, f) != 2) return false;
    *out = buf[0] | (buf[1] << 8);
    return true;
}

bool wav_read(const char *path, WavFile *out) {
    memset(out, 0, sizeof(*out));

    FILE *f = fopen(path, "rb");
    if (!f) return false;

    // RIFF header
    char riff_id[4];
    uint32_t file_size;
    char wave_id[4];
    if (fread(riff_id, 1, 4, f) != 4 || memcmp(riff_id, "RIFF", 4) != 0) goto fail;
    if (!read_u32(f, &file_size)) goto fail;
    if (fread(wave_id, 1, 4, f) != 4 || memcmp(wave_id, "WAVE", 4) != 0) goto fail;

    uint16_t audio_format = 0, num_channels = 0, bits_per_sample = 0;
    uint32_t sample_rate = 0;
    bool found_fmt = false, found_data = false;
    float *samples = NULL;
    uint32_t num_frames = 0;

    while (!found_data) {
        char chunk_id[4];
        uint32_t chunk_size;
        if (fread(chunk_id, 1, 4, f) != 4) goto fail;
        if (!read_u32(f, &chunk_size)) goto fail;

        if (memcmp(chunk_id, "fmt ", 4) == 0) {
            if (!read_u16(f, &audio_format)) goto fail;
            if (!read_u16(f, &num_channels)) goto fail;
            if (!read_u32(f, &sample_rate)) goto fail;
            uint32_t byte_rate; if (!read_u32(f, &byte_rate)) goto fail;
            uint16_t block_align; if (!read_u16(f, &block_align)) goto fail;
            if (!read_u16(f, &bits_per_sample)) goto fail;
            // Skip any extra fmt bytes
            if (chunk_size > 16)
                fseek(f, chunk_size - 16, SEEK_CUR);
            found_fmt = true;
        } else if (memcmp(chunk_id, "data", 4) == 0) {
            if (!found_fmt) goto fail;
            if (audio_format != 1) goto fail; // PCM only

            uint32_t bytes_per_sample = bits_per_sample / 8;
            uint32_t frame_size = bytes_per_sample * num_channels;
            num_frames = chunk_size / frame_size;

            samples = malloc(num_frames * num_channels * sizeof(float));
            if (!samples) goto fail;

            if (bits_per_sample == 24) {
                uint8_t *raw = malloc(chunk_size);
                if (!raw) { free(samples); goto fail; }
                if (fread(raw, 1, chunk_size, f) != chunk_size) {
                    free(raw); free(samples); goto fail;
                }
                for (uint32_t i = 0; i < num_frames * num_channels; i++) {
                    int32_t val = raw[i*3] | (raw[i*3+1] << 8) | (raw[i*3+2] << 16);
                    if (val & 0x800000) val |= 0xFF000000; // sign extend
                    samples[i] = val / 8388608.0f;
                }
                free(raw);
            } else if (bits_per_sample == 16) {
                uint8_t *raw = malloc(chunk_size);
                if (!raw) { free(samples); goto fail; }
                if (fread(raw, 1, chunk_size, f) != chunk_size) {
                    free(raw); free(samples); goto fail;
                }
                for (uint32_t i = 0; i < num_frames * num_channels; i++) {
                    int16_t val = (int16_t)(raw[i*2] | (raw[i*2+1] << 8));
                    samples[i] = val / 32768.0f;
                }
                free(raw);
            } else if (bits_per_sample == 32) {
                // 32-bit float
                if (fread(samples, sizeof(float), num_frames * num_channels, f)
                    != num_frames * num_channels) {
                    free(samples); goto fail;
                }
            } else {
                free(samples); goto fail;
            }
            found_data = true;
        } else {
            // Skip unknown chunk
            fseek(f, chunk_size, SEEK_CUR);
        }
    }

    fclose(f);
    out->samples = samples;
    out->num_frames = num_frames;
    out->sample_rate = sample_rate;
    out->num_channels = num_channels;
    return true;

fail:
    fclose(f);
    return false;
}

void wav_free(WavFile *wav) {
    free(wav->samples);
    wav->samples = NULL;
    wav->num_frames = 0;
}
