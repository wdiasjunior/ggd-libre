"""
NCW (Native Instruments Compressed Wave) decoder.

Format: lossless compression using DPCM (delta) and bit truncation.
Based on reverse-engineering work from github.com/monomadic/ncw

Header: 120 bytes
  - 8 bytes magic
  - u16 channels
  - u16 bits_per_sample
  - u32 sample_rate
  - u32 num_samples (frames)
  - u32 blocks_offset (where block offset table starts)
  - u32 data_offset (where compressed data starts)
  - u32 data_size

Block offset table: (data_offset - blocks_offset) / 4 entries, each u32
  - Each value is an offset relative to data_offset

Per block, per channel:
  Block header (16 bytes):
    - u32 magic 0x160C9A3E (big-endian)
    - i32 base_value (LE)
    - i16 bits (LE) - positive=delta, negative=truncated, zero=raw
    - u16 flags (LE) - bit0: mid/side, bit1: float format
  Block data: abs(bits) * 64 bytes
  Samples per block: 512
"""

import struct
import numpy as np
from dataclasses import dataclass
from typing import BinaryIO


HEADER_SIZE = 120
BLOCK_HEADER_SIZE = 16
SAMPLES_PER_BLOCK = 512
BLOCK_HEADER_MAGIC = 0x160C9A3E


@dataclass
class NcwHeader:
    channels: int
    bits_per_sample: int
    sample_rate: int
    num_samples: int
    blocks_offset: int
    data_offset: int
    data_size: int


@dataclass
class BlockHeader:
    base_value: int  # i32
    bits: int        # i16 (signed - determines encoding mode)
    flags: int       # u16


def read_header(f: BinaryIO) -> NcwHeader:
    """Read and parse the 120-byte NCW file header."""
    data = f.read(HEADER_SIZE)
    if len(data) < HEADER_SIZE:
        raise ValueError("File too short for NCW header")

    magic = struct.unpack_from('>Q', data, 0)[0]
    valid_magics = (0x01A89ED631010000, 0x01A89ED630010000)
    if magic not in valid_magics:
        raise ValueError(f"Invalid NCW magic: 0x{magic:016X}")

    channels, bits_per_sample = struct.unpack_from('<HH', data, 8)
    sample_rate, num_samples = struct.unpack_from('<II', data, 12)
    blocks_offset, data_offset, data_size = struct.unpack_from('<III', data, 20)

    return NcwHeader(
        channels=channels,
        bits_per_sample=bits_per_sample,
        sample_rate=sample_rate,
        num_samples=num_samples,
        blocks_offset=blocks_offset,
        data_offset=data_offset,
        data_size=data_size,
    )


def read_block_offsets(f: BinaryIO, header: NcwHeader) -> list[int]:
    """Read the block offset table."""
    f.seek(header.blocks_offset)
    num_entries = (header.data_offset - header.blocks_offset) // 4
    # The reference impl reads num_entries - 1 offsets
    offsets = []
    for _ in range(num_entries - 1):
        offsets.append(struct.unpack('<I', f.read(4))[0])
    return offsets


def read_block_header(f: BinaryIO) -> BlockHeader:
    """Read a 16-byte block header."""
    data = f.read(BLOCK_HEADER_SIZE)
    if len(data) < BLOCK_HEADER_SIZE:
        raise ValueError("Unexpected EOF reading block header")

    magic = struct.unpack_from('>I', data, 0)[0]
    if magic != BLOCK_HEADER_MAGIC:
        raise ValueError(f"Invalid block magic: 0x{magic:08X} at position {f.tell() - 16}")

    base_value = struct.unpack_from('<i', data, 4)[0]
    bits = struct.unpack_from('<h', data, 8)[0]
    flags = struct.unpack_from('<H', data, 10)[0]

    return BlockHeader(base_value=base_value, bits=bits, flags=flags)


def unpack_signed_values(data: bytes, bit_width: int) -> np.ndarray:
    """Unpack bit-packed signed integers from byte data."""
    values = []
    bit_accumulator = 0
    bits_in_acc = 0
    byte_idx = 0
    mask = (1 << bit_width) - 1
    sign_bit = 1 << (bit_width - 1)

    while byte_idx < len(data):
        bit_accumulator |= data[byte_idx] << bits_in_acc
        bits_in_acc += 8
        byte_idx += 1

        while bits_in_acc >= bit_width:
            value = bit_accumulator & mask
            # Sign extend
            if value & sign_bit:
                value |= ~mask  # Python handles arbitrary precision
                value = -((~value + 1) & 0xFFFFFFFF) if value < 0 else value
                # Simpler: use struct-style sign extension
                value = value - (1 << bit_width) if value >= sign_bit else value
            values.append(value)
            bit_accumulator >>= bit_width
            bits_in_acc -= bit_width

    return np.array(values, dtype=np.int32)


def decode_delta_block(base_value: int, data: bytes, bit_width: int) -> np.ndarray:
    """Decode a DPCM (delta) encoded block."""
    deltas = unpack_signed_values(data, bit_width)
    # Build samples: first sample is base_value, each subsequent adds delta
    samples = np.empty(SAMPLES_PER_BLOCK, dtype=np.int32)
    samples[0] = base_value
    # The reference puts base at each position and accumulates differently:
    # samples[i] = prev_base; prev_base += delta[i]
    # This means sample[0] = base_value, sample[1] = base_value + delta[0], etc.
    # Actually looking more carefully: samples[i] = prev_base; prev_base += delta
    # So it uses deltas[0..511], and the first sample IS base_value before any delta applied.
    prev = np.int64(base_value)
    for i in range(SAMPLES_PER_BLOCK):
        samples[i] = np.int32(prev & 0xFFFFFFFF)
        if i < len(deltas):
            prev += deltas[i]
    return samples


def decode_truncated_block(data: bytes, bit_width: int) -> np.ndarray:
    """Decode a bit-truncated block (samples stored at reduced precision)."""
    # These are unsigned packed values (not sign-extended in reference)
    values = []
    bit_offset = 0
    total_bits = len(data) * 8
    mask = (1 << bit_width) - 1

    while bit_offset + bit_width <= total_bits and len(values) < SAMPLES_PER_BLOCK:
        byte_offset = bit_offset // 8
        bit_remainder = bit_offset % 8

        # Read enough bytes to cover the value
        temp = 0
        for i in range((bit_width + bit_remainder + 7) // 8):
            if byte_offset + i < len(data):
                temp |= data[byte_offset + i] << (i * 8)

        value = (temp >> bit_remainder) & mask
        values.append(value)
        bit_offset += bit_width

    result = np.array(values, dtype=np.int32)
    # Pad to SAMPLES_PER_BLOCK if needed
    if len(result) < SAMPLES_PER_BLOCK:
        result = np.pad(result, (0, SAMPLES_PER_BLOCK - len(result)))
    return result


def decode_raw_block(f: BinaryIO, bits_per_sample: int) -> np.ndarray:
    """Decode an uncompressed block."""
    bytes_per_sample = bits_per_sample // 8
    samples = np.empty(SAMPLES_PER_BLOCK, dtype=np.int32)

    for i in range(SAMPLES_PER_BLOCK):
        sample_bytes = f.read(bytes_per_sample)
        if len(sample_bytes) < bytes_per_sample:
            samples[i] = 0
            continue
        # Pad to 4 bytes for i32 conversion
        padded = sample_bytes + b'\x00' * (4 - bytes_per_sample)
        val = struct.unpack('<i', padded)[0]
        # Sign extend for 24-bit
        if bits_per_sample == 24:
            if val & 0x800000:
                val |= ~0xFFFFFF
        samples[i] = val

    return samples


def decode_ncw(filepath: str) -> tuple[NcwHeader, np.ndarray]:
    """
    Decode an NCW file to raw PCM samples.

    Returns (header, samples) where samples is interleaved int32.
    """
    with open(filepath, 'rb') as f:
        header = read_header(f)
        block_offsets = read_block_offsets(f, header)

        # Each block carries SAMPLES_PER_BLOCK samples *per channel*, so the
        # final partial block is measured in frames, not interleaved samples.
        # Dividing the interleaved count by channels under-counts on stereo
        # files and used to leave the tail of every stereo sample unwritten.
        overflow_samples = header.num_samples % SAMPLES_PER_BLOCK

        # Decode per-channel then interleave
        channels = [[] for _ in range(header.channels)]

        for block_idx, offset in enumerate(block_offsets):
            is_final = (block_idx == len(block_offsets) - 1)

            # Seek to block start
            f.seek(header.data_offset + offset)

            block_samples = []
            for ch in range(header.channels):
                block_hdr = read_block_header(f)
                if ch == 0:
                    # Kontakt takes the mid/side transform from the first
                    # channel's flags only.
                    mid_side = bool(block_hdr.flags & 1)
                bit_width = abs(block_hdr.bits)

                if block_hdr.bits > 0:
                    # Delta (DPCM) encoding
                    data = f.read(bit_width * 64)
                    samples = decode_delta_block(block_hdr.base_value, data, bit_width)
                elif block_hdr.bits < 0:
                    # Bit truncation
                    data = f.read(bit_width * 64)
                    samples = decode_truncated_block(data, bit_width)
                else:
                    # Uncompressed
                    bytes_per_sample = header.bits_per_sample // 8
                    data = f.read(bytes_per_sample * SAMPLES_PER_BLOCK)
                    if bytes_per_sample == 3:
                        # numpy has no int24: assemble little-endian bytes and
                        # sign-extend from bit 23.
                        b = np.frombuffer(data, dtype=np.uint8).reshape(-1, 3).astype(np.int32)
                        samples = b[:, 0] | (b[:, 1] << 8) | (b[:, 2] << 16)
                        samples = np.where(samples & 0x800000, samples - 0x1000000, samples)
                    else:
                        samples = np.frombuffer(data, dtype=np.int32 if bytes_per_sample == 4
                                               else np.dtype(f'<i{bytes_per_sample}'))
                    if bytes_per_sample < 4:
                        samples = samples.astype(np.int32)

                # Trim final block
                if is_final and overflow_samples > 0:
                    samples = samples[:overflow_samples]

                block_samples.append(samples.astype(np.int32))

            if mid_side and header.channels == 2:
                # Sub-block 0 is mid, sub-block 1 is side (wrapping int32 math).
                mid, side = block_samples
                block_samples = [mid + side, mid - side]

            for ch in range(header.channels):
                channels[ch].append(block_samples[ch])

    # Interleave channels
    num_frames = header.num_samples
    # zeros, not empty: any frame the decode failed to produce must be silence
    # rather than uninitialized memory (which reads as full-scale noise).
    planar = np.zeros((num_frames, header.channels), dtype=np.int32)
    for ch in range(header.channels):
        data = np.concatenate(channels[ch]) if channels[ch] else np.zeros(0, np.int32)
        n = min(len(data), num_frames)
        planar[:n, ch] = data[:n]
    interleaved = planar.reshape(-1)

    return header, interleaved


if __name__ == '__main__':
    import sys
    if len(sys.argv) < 2:
        print("Usage: python ncw_decoder.py <file.ncw>")
        sys.exit(1)

    header, samples = decode_ncw(sys.argv[1])
    print(f"Channels: {header.channels}")
    print(f"Sample rate: {header.sample_rate}")
    print(f"Bits per sample: {header.bits_per_sample}")
    print(f"Total frames: {header.num_samples}")
    print(f"Decoded {len(samples)} samples")
    print(f"First 10 samples: {samples[:10]}")
