#!/usr/bin/env python3
"""
Measure the recorded level of each velocity layer.

The plugin's audio engine assumes the recorded velocity layers themselves carry
the dynamics (a dyn1 hit is intrinsically quieter than a dyn10 hit), so velocity
should only select the layer and not additionally scale the volume.

This script verifies that assumption. Run it before changing the velocity curve.

  Spread > ~15 dB from the softest to the loudest layer
      -> layers are unnormalized, velocity must NOT apply extra gain.
  Spread < ~5 dB
      -> layers are normalized, the engine still needs velocity->volume tracking.

Reads NCW directly from the Kontakt pack, or WAV from a converted output tree.

Usage:
    python measure_layers.py <samples_dir> [articulation ...]
"""

import math
import sys
import wave
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from ncw_decoder import decode_ncw

DEFAULT_ARTICULATIONS = [
    "Kick22x16_MainHit",
    "SnareHigh",
    "ClearTom10",
    "17ByzThinCrash_MainHit",
    "HiHat_TipClosed",
]

# Mic to prefer per articulation; crashes have no close mic.
MIC_PREFERENCE = ["CloseMic", "OHMic", "TopMic1", "NearRoomMic"]


def db(x: float) -> float:
    return 20.0 * math.log10(x) if x > 1e-12 else -float("inf")


def read_samples(path: Path):
    """Return a list of floats in -1..1 from an .ncw or .wav file."""
    if path.suffix.lower() == ".ncw":
        header, interleaved = decode_ncw(str(path))
        full = float(1 << (header.bits_per_sample - 1))
        return [s / full for s in interleaved]

    with wave.open(str(path), "rb") as w:
        width = w.getsampwidth()
        raw = w.readframes(w.getnframes())
    full = float(1 << (width * 8 - 1))
    out = []
    for i in range(0, len(raw) - width + 1, width):
        val = int.from_bytes(raw[i : i + width], "little", signed=True)
        out.append(val / full)
    return out


def measure(path: Path):
    samples = read_samples(path)
    if not samples:
        return None
    peak = max(abs(s) for s in samples)
    rms = math.sqrt(sum(s * s for s in samples) / len(samples))
    return peak, rms, len(samples)


def find_layers(root: Path, articulation: str):
    """Yield (dyn, path) for dyn1..N of the first mic position that has them."""
    for mic in MIC_PREFERENCE:
        # NCW pack: flat directory, files named {Mic}_{Art}_dyn{N}_rr1.ncw
        # WAV tree: root/{Mic}/{Mic}_{Art}_dyn{N}_rr1.wav
        for base, ext in ((root / mic, ".wav"), (root, ".ncw")):
            if not base.is_dir():
                continue
            found = []
            for dyn in range(1, 17):
                p = base / f"{mic}_{articulation}_dyn{dyn}_rr1{ext}"
                if p.exists():
                    found.append((dyn, p))
            if found:
                return mic, found
    return None, []


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1

    root = Path(sys.argv[1])
    if not root.is_dir():
        print(f"error: not a directory: {root}")
        return 1

    articulations = sys.argv[2:] or DEFAULT_ARTICULATIONS

    for art in articulations:
        mic, layers = find_layers(root, art)
        if not layers:
            print(f"\n{art}: no layers found")
            continue

        print(f"\n{art}  [{mic}]")
        print(f"  {'layer':<7}{'peak dBFS':>12}{'rms dBFS':>12}{'frames':>10}")

        results = []
        for dyn, path in layers:
            m = measure(path)
            if not m:
                continue
            peak, rms, n = m
            results.append((dyn, db(peak), db(rms)))
            print(f"  dyn{dyn:<4}{db(peak):>12.2f}{db(rms):>12.2f}{n:>10}")

        if len(results) >= 2:
            peak_spread = results[-1][1] - results[0][1]
            rms_spread = results[-1][2] - results[0][2]
            print(f"  {'spread':<7}{peak_spread:>12.2f}{rms_spread:>12.2f}")
            if rms_spread > 15.0:
                verdict = "UNNORMALIZED - velocity should only select the layer"
            elif rms_spread < 5.0:
                verdict = "NORMALIZED - engine still needs velocity->volume tracking"
            else:
                verdict = "PARTIAL - needs a judgement call"
            print(f"  -> {verdict}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
