#include "wav_reader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

static uint32_t read_le32(const uint8_t *p) {
    return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint16_t read_le16(const uint8_t *p) {
    return p[0] | (p[1] << 8);
}

bool wav_open(const char *path, WavFile *out) {
    memset(out, 0, sizeof(*out));

#ifdef _WIN32
    // Open file
    HANDLE hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                                OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    LARGE_INTEGER fsize;
    if (!GetFileSizeEx(hFile, &fsize)) { CloseHandle(hFile); return false; }
    size_t file_size = (size_t)fsize.QuadPart;

    HANDLE hMap = CreateFileMappingA(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
    if (!hMap) { CloseHandle(hFile); return false; }

    void *base = MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);
    if (!base) { CloseHandle(hMap); CloseHandle(hFile); return false; }

    out->map_base = base;
    out->map_size = file_size;
    out->file_handle = hFile;
    out->map_handle = hMap;
#else
    int fd = open(path, O_RDONLY);
    if (fd < 0) return false;

    struct stat st;
    if (fstat(fd, &st) < 0) { close(fd); return false; }
    size_t file_size = (size_t)st.st_size;

    void *base = mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd); // fd can be closed after mmap
    if (base == MAP_FAILED) return false;

    out->map_base = base;
    out->map_size = file_size;
#endif

    // Parse WAV header from mapped memory
    const uint8_t *data = (const uint8_t *)base;

    if (file_size < 44) goto fail;
    if (memcmp(data, "RIFF", 4) != 0) goto fail;
    if (memcmp(data + 8, "WAVE", 4) != 0) goto fail;

    size_t pos = 12;
    bool found_fmt = false;

    while (pos + 8 <= file_size) {
        const uint8_t *chunk = data + pos;
        uint32_t chunk_size = read_le32(chunk + 4);

        if (memcmp(chunk, "fmt ", 4) == 0 && pos + 8 + chunk_size <= file_size) {
            if (chunk_size < 16) goto fail;
            const uint8_t *fmt = chunk + 8;
            uint16_t audio_format = read_le16(fmt);
            if (audio_format != 1) goto fail; // PCM only

            out->num_channels = read_le16(fmt + 2);
            out->sample_rate = read_le32(fmt + 4);
            out->bits_per_sample = read_le16(fmt + 14);
            found_fmt = true;
        } else if (memcmp(chunk, "data", 4) == 0 && found_fmt) {
            uint32_t bytes_per_sample = out->bits_per_sample / 8;
            uint32_t frame_size = bytes_per_sample * out->num_channels;
            out->num_frames = chunk_size / frame_size;
            out->pcm_data = chunk + 8;

            // Verify the data doesn't extend past the file
            if ((size_t)(out->pcm_data - data) + chunk_size > file_size)
                out->num_frames = (uint32_t)((file_size - (out->pcm_data - data)) / frame_size);

            return true;
        }

        pos += 8 + chunk_size;
        if (chunk_size & 1) pos++; // WAV chunks are word-aligned
    }

fail:
    wav_close(out);
    return false;
}

void wav_close(WavFile *wav) {
    if (!wav->map_base) return;

#ifdef _WIN32
    UnmapViewOfFile(wav->map_base);
    if (wav->map_handle) CloseHandle(wav->map_handle);
    if (wav->file_handle) CloseHandle(wav->file_handle);
    wav->map_handle = NULL;
    wav->file_handle = NULL;
#else
    munmap(wav->map_base, wav->map_size);
#endif

    wav->map_base = NULL;
    wav->pcm_data = NULL;
    wav->map_size = 0;
    wav->num_frames = 0;
}
