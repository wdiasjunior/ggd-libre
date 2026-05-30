#ifndef GGD_WAV_READER_H
#define GGD_WAV_READER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Memory-mapped WAV file — no data is copied into RAM,
// the OS pages in samples on demand from disk.
typedef struct {
    void     *map_base;      // mmap/MapViewOfFile base pointer
    size_t    map_size;       // total mapped size
    const uint8_t *pcm_data; // pointer into map_base at the data chunk
    uint32_t  num_frames;
    uint32_t  sample_rate;
    uint16_t  num_channels;
    uint16_t  bits_per_sample;
#ifdef _WIN32
    void     *file_handle;   // HANDLE for CreateFileMapping
    void     *map_handle;    // HANDLE for the mapping object
#endif
} WavFile;

// Parse WAV header and mmap the file. No sample data is read into memory.
bool wav_open(const char *path, WavFile *out);

// Unmap the file.
void wav_close(WavFile *wav);

// Read a single sample as float. Inline for performance.
static inline float wav_sample_at(const WavFile *wav, uint32_t frame) {
    // All our samples are 24-bit mono PCM
    const uint8_t *p = wav->pcm_data + frame * 3;
    int32_t val = p[0] | (p[1] << 8) | (p[2] << 16);
    if (val & 0x800000) val |= (int32_t)0xFF000000; // sign extend
    return val / 8388608.0f;
}

#endif
