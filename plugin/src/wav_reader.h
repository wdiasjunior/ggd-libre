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
    uint16_t  frame_stride;  // bytes per frame = (bits/8) * channels
#ifdef _WIN32
    void     *file_handle;   // HANDLE for CreateFileMapping
    void     *map_handle;    // HANDLE for the mapping object
#endif
} WavFile;

// Parse WAV header and mmap the file. No sample data is read into memory.
bool wav_open(const char *path, WavFile *out);

// Unmap the file.
void wav_close(WavFile *wav);

// Decode one 24-bit little-endian sample at a raw byte offset.
static inline float wav_decode24(const uint8_t *p) {
    int32_t val = p[0] | (p[1] << 8) | ((int32_t)p[2] << 16);
    if (val & 0x800000) val |= (int32_t)0xFF000000; // sign extend
    return val * (1.0f / 8388608.0f);
}

// Read one frame as a stereo pair.
// Mono sources yield the same value on both sides; stereo sources yield L and R.
// The room and overhead mics in this library are stereo, the spot mics are mono,
// so every read has to go through the frame stride rather than assuming mono.
static inline void wav_frame_lr(const WavFile *wav, uint32_t frame,
                                float *out_l, float *out_r) {
    const uint8_t *p = wav->pcm_data + (size_t)frame * wav->frame_stride;
    float l = wav_decode24(p);
    *out_l = l;
    *out_r = (wav->num_channels >= 2) ? wav_decode24(p + 3) : l;
}

// Read a single sample as float (channel 0). Kept for callers that only
// need a mono view of the source.
static inline float wav_sample_at(const WavFile *wav, uint32_t frame) {
    return wav_decode24(wav->pcm_data + (size_t)frame * wav->frame_stride);
}

#endif
