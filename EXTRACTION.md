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
    "../output/halpern"
```

This will:
- Decode all 8,608 NCW files to 24-bit WAV (48kHz mono)
- Organize them by mic position: `output/halpern/wav/CloseMic/`, `output/halpern/wav/OHMic/`, etc.
- Generate `output/halpern/sample_inventory.json` with metadata

Takes ~5-10 minutes depending on CPU (uses all cores).

### Step 3: Generate MIDI mapping

```bash
python generate_mapping.py \
    "../output/halpern/sample_inventory.json" \
    "../output/halpern/midi_map.json"
```

This creates `midi_map.json` mapping MIDI notes 24-83 to sample articulations.

Then build the index the plugin loads:

```bash
python build_index.py halpern ../output/halpern
```

### Step 4: Generate SFZ files (optional)

```bash
python generate_sfz.py \
    "../output/halpern/midi_map.json" \
    "../output/halpern/wav" \
    "../output/halpern/sfz"
```

### Step 5: Install for the CLAP plugin

Copy the library folder into `ggd-libre-data/` next to the `.clap` file (see
[Installing](README.md#installing)):

```bash
mkdir -p ~/.clap/ggd-libre-data
cp -r output/halpern ~/.clap/ggd-libre-data/
cp plugin/ggd-libre-linux.clap ~/.clap/
```

The plugin only needs `wav/` and `library_index.json`; `midi_map.json` is
used by `build_index.py`. Installs from earlier versions, in
`GGD Matt Halpern Signature Pack/` next to the `.clap` file or pointed to by
`GGD_SAMPLES_PATH`, still work as long as that folder also has
`library_index.json`.

## GGD One Kit Wonder Metal / GGD PV Matt Halpern Signature Pack

> **Legal notice.** ggd-libre does not condone piracy and is not affiliated
> with Getgood Drums or Native Instruments. These two libraries ship their
> samples inside encrypted NKX containers. This repository does **not**
> include, describe or link to any way of decrypting them, and issues or pull
> requests asking for one will be closed. Use only samples you are licensed
> to use, follow the library EULA, and never redistribute the WAV files or
> anything derived from them.

The steps below start from WAV files you already have and are allowed to use.
How you get them is up to you and your license. If you are unsure, ask
Getgood Drums. Once you have the WAVs, the steps match the Halpern pack from
Step 3 onward. The only difference is that `build_index.py` parses the GGD
file names directly, so these libraries don't need a `midi_map.json`.

| | One Kit Wonder Metal | PV Matt Halpern Signature Pack |
|---|---|---|
| Library slug | `okw_metal` | `pv_halpern` |
| File prefix | `GGD-OKW3-` | `GGD-PV-` |
| Samples | 4,652 | 13,902 |
| WAV size | ~6.5 GB | ~14 GB |
| Mic channels | `Cls`, `OH`, `Rm` / `RmMS` | `KckIn`, `KckOut`, `KckPrt`, `SnrTop`, `SnrBtm`, `Cls`, `OH`, `RmClsFOK`, `RmFarBlm`, `RmFarWde` |

### Step 1: Locate the Kontakt library

The libraries look like this. The `.nkx` files are encrypted, so the tools
in this repository cannot read them:

```
GGD One Kit Wonder Metal/
├── One Kit Wonder - Metal.nicnt
├── Instruments/One Kit Wonder - Metal.nki
└── Samples/
    ├── OneKitWonder_Metal_0.nkx / .nkc
    └── OneKitWonder_Metal_1.nkx / .nkc

GGD PV Matt Halpern Signature Pack/
├── P5 Matt Halpern Signature Pack.nicnt
├── Instruments/P5 Matt Halpern Signature Pack.nki
└── Samples/
    └── P5_Matt_Halpern_Signature_Pack_{0..3}.nkx / .nkc
```

### Step 2: Arrange your WAV files

Put the WAVs under `output/<slug>/wav/`, one folder per drum piece, and keep
the original GGD file names:

```
output/okw_metal/
└── wav/
    ├── 14x8PrlVPSigSnr/
    │   └── GGD-OKW3-14x8PrlVPSigSnr-Eb-HitCtr-Cls-Sum-RR1-V1.wav
    ├── 22x16TamaStclMplKck/
    └── ...

output/pv_halpern/
└── wav/
    ├── 14x6PrlMHSigSnr/
    │   └── GGD-PV-14x6PrlMHSigSnr-Hgh-F#-CrsStk-OH-KM85i-RR1-V10.wav
    ├── 22x18PrlRefKck/
    └── ...
```

The names follow this pattern:

```
GGD-<OKW3|PV>-<Piece>[-<Qualifiers>]-<Articulation>-<Mic>[-<MicModel>]-RR<n>-V<n>.wav
```

- `<Piece>`: drum or cymbal, e.g. `14x8PrlVPSigSnr`. This is also the folder name.
- `<Qualifiers>`: optional, e.g. the snare tuning (`Low-D`, `Mid-E`, `Hgh-F#`).
- `<Mic>`: one of the mic tokens in the table above.
- `RR<n>`: round-robin index. `V<n>`: velocity layer.

`build_index.py` scans `wav/` recursively, so it only needs the file names to
be right. The folder layout just keeps things tidy. Files must be 24-bit PCM
WAV (mono or stereo, 44.1 or 48 kHz).

### Step 3: Build the library index

```bash
cd tools/
python build_index.py okw_metal  ../output/okw_metal
python build_index.py pv_halpern ../output/pv_halpern
```

This writes `library_index.json` next to `wav/`. The plugin loads it to map
MIDI notes, velocity layers, round robins and kit-piece selectors (crashes,
snare tuning, kick, ride, ...). All three kits use the same note layout
(24-83, plus 31 for snare rimshot), so one MIDI part plays any of them. If a
sample the mapping needs is missing, the script prints `ERROR:` lines and
writes nothing.

### Step 4: Install for the CLAP plugin

The plugin shows one tab per library in the bar at the top of its window.
Libraries it cannot find are greyed out and marked "not extracted"; clicking
one re-scans the disk.

Copy each library folder into `ggd-libre-data/` next to the `.clap` file,
as for the Halpern pack:

```bash
cp -r output/okw_metal output/pv_halpern ~/.clap/ggd-libre-data/
```

See [Installing](README.md#installing) for the full layout and the
`GGD_LIBRE_ROOT` override.

## File format reference

### NCW (Native Instruments Compressed Wave)
- Lossless compression using DPCM and bit truncation
- 120-byte header with channel count, bit depth, sample rate, frame count
- Block-based compression (512 samples per block)
- Decoded by `tools/ncw_decoder.py`

### NKX (Kontakt Container)
- Proprietary encrypted container bundling multiple NCW files
- Directory section at the start lists filenames as UTF-16LE strings
- Data section is encrypted; this project does not read it and does not document how
- Catalog stored in companion `.nkc` files
