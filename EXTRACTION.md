# Sample Extraction Guide

This document explains how to extract drum samples from GGD Kontakt libraries for use with the GGD Libre CLAP plugins.

## Prerequisites

- Python 3.8+
- The original Kontakt library files
- ~10GB free disk space per library

## GGD Matt Halpern Signature Pack

This pack stores samples as individual `.ncw` files, which can be directly decoded.

### Step 1: Locate the Kontakt library

Find the directory containing:
```
GGD Matt Halpern Signature Pack KONTAKT/
├── Halpern Drums.nki
├── Halpern Drums.nkc
├── Halpern Drums.nkr
└── Halpern Drums Samples/
    ├── BottomMic_13Snare_dyn1_rr1.ncw
    ├── CloseMic_Kick22x16_MainHit_dyn1_rr1.ncw
    └── ... (8,608 .ncw files)
```

### Step 2: Convert NCW to WAV

```bash
cd tools/
python batch_convert.py \
    "../GGD Matt Halpern Signature Pack KONTAKT/Halpern Drums Samples" \
    "../output"
```

This will:
- Decode all 8,608 NCW files to 24-bit WAV (48kHz mono)
- Organize them by mic position: `output/wav/CloseMic/`, `output/wav/OHMic/`, etc.
- Generate `output/sample_inventory.json` with metadata

Takes ~5-10 minutes depending on CPU (uses all cores).

### Step 3: Generate MIDI mapping

```bash
python generate_mapping.py \
    "../output/sample_inventory.json" \
    "../output/midi_map.json"
```

This creates `midi_map.json` mapping MIDI notes 24-83 to sample articulations.

### Step 4: Generate SFZ files (optional)

```bash
python generate_sfz.py \
    "../output/midi_map.json" \
    "../output/wav" \
    "../output/sfz"
```

### Step 5: Install for the CLAP plugin

Copy the output to the plugin's sample directory:

**Linux:**
```bash
mkdir -p ~/.clap/GGD\ Matt\ Halpern\ Signature\ Pack/
cp -r output/wav output/midi_map.json \
    ~/.clap/GGD\ Matt\ Halpern\ Signature\ Pack/
cp plugin/ggd-libre-linux.clap ~/.clap/
```

**Windows:**
Copy to the same directory as the `.clap` file:
```
C:\Program Files\Common Files\CLAP\
├── ggd-libre-windows.clap
└── GGD Matt Halpern Signature Pack\
    ├── midi_map.json
    └── wav\
        ├── CloseMic\
        ├── OHMic\
        └── ...
```

Or set the environment variable `GGD_SAMPLES_PATH` to point to the `wav/` directory.

## GGD One Kit Wonder Metal

**Status: Not currently supported.**

This pack uses encrypted NKX container files which cannot be extracted without Kontakt. The samples are stored in:
```
Getgood Drums One Kit Wonder Metal KONTAKT/
└── Samples/
    ├── OneKitWonder_Metal_0.nkx  (2.0 GB, encrypted)
    ├── OneKitWonder_Metal_1.nkx  (600 MB, encrypted)
    ├── OneKitWonder_Metal_0.nkc  (catalog)
    └── OneKitWonder_Metal_1.nkc  (catalog)
```

The NKX files contain 4,652 NCW samples with 14 drum pieces and 4 mic positions, but the data is encrypted by Kontakt's DRM.

### If you have Kontakt

If you own Kontakt, you can extract the samples using Kontakt's batch re-save:

1. Open the `.nki` file in Kontakt
2. Go to **File > Batch re-save**
3. This decrypts and saves the samples as individual files
4. The extracted files can then be processed with adapted extraction scripts

### Sample naming convention (for reference)

OKW Metal uses a different naming pattern than Halpern:
```
GGD-OKW3-{Drum}-{Articulation}-{MicPosition}-{MicModel}-RR{N}-V{N}.ncw
```

Example: `GGD-OKW3-14x8PrlVPSigSnr-Eb-HitCtr-Cls-Sum-RR1-V1.ncw`

Mic positions: `Cls` (Close), `OH` (Overhead), `Rm` (Room), `RmMS` (Room Mid-Side)

## File format reference

### NCW (Native Instruments Compressed Wave)
- Lossless compression using DPCM and bit truncation
- 120-byte header with channel count, bit depth, sample rate, frame count
- Block-based compression (512 samples per block)
- Decoded by `tools/ncw_decoder.py`

### NKX (Kontakt Container)
- Proprietary encrypted container bundling multiple NCW files
- Directory section at the start lists filenames as UTF-16LE strings
- Data section is encrypted — requires Kontakt for extraction
- Catalog stored in companion `.nkc` files
