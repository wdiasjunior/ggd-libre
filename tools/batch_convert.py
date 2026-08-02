#!/usr/bin/env python3
"""
Batch convert NCW files to WAV with parallel processing.
Organizes output by microphone position.
"""

import os
import sys
import wave
import struct
import json
from pathlib import Path
from concurrent.futures import ProcessPoolExecutor, as_completed
from dataclasses import dataclass, asdict
import argparse
import time


def convert_single(ncw_path: str, output_dir: str) -> tuple[str, bool, str]:
    """Convert a single NCW file to WAV. Returns (filename, success, message)."""
    # Import here so each worker process has its own import
    sys.path.insert(0, str(Path(__file__).parent))
    from ncw_decoder import decode_ncw

    try:
        ncw_path = Path(ncw_path)
        output_dir = Path(output_dir)

        # Parse mic position from filename
        name = ncw_path.stem
        parts = name.split('_')
        mic_position = parts[0]  # e.g. CloseMic, OHMic, NearRoomMic

        # Create mic position subdirectory
        mic_dir = output_dir / mic_position
        mic_dir.mkdir(parents=True, exist_ok=True)

        wav_path = mic_dir / f"{name}.wav"

        # Skip if already converted
        if wav_path.exists():
            return (name, True, "skipped (exists)")

        # Decode NCW
        header, samples = decode_ncw(str(ncw_path))

        # Write WAV
        with wave.open(str(wav_path), 'wb') as wf:
            wf.setnchannels(header.channels)
            wf.setsampwidth(header.bits_per_sample // 8)
            wf.setframerate(header.sample_rate)

            # Pack samples as bytes
            if header.bits_per_sample == 24:
                # A handful of files decode to values outside the signed 24-bit
                # range. Masking those to 24 bits wraps a positive overshoot
                # round to full-scale negative, which is an audible click, so
                # clamp instead.
                import numpy as np
                clamped = np.clip(np.asarray(samples, dtype=np.int64),
                                  -8388608, 8388607).astype('<i4')
                # Drop the high byte of each little-endian int32 to get 24-bit.
                raw = clamped.view(np.uint8).reshape(-1, 4)[:, :3].tobytes()
                wf.writeframes(raw)
            elif header.bits_per_sample == 16:
                raw = struct.pack(f'<{len(samples)}h', *samples.astype('int16'))
                wf.writeframes(raw)
            elif header.bits_per_sample == 32:
                raw = struct.pack(f'<{len(samples)}i', *samples)
                wf.writeframes(raw)

        return (name, True, "ok")

    except Exception as e:
        return (ncw_path if isinstance(ncw_path, str) else str(ncw_path), False, str(e))


def build_inventory(samples_dir: Path) -> dict:
    """Parse all filenames into a structured inventory."""
    inventory = {
        'mic_positions': set(),
        'drums': {},  # drum_name -> {max_dyn, max_rr, mics}
        'total_files': 0,
    }

    for ncw_file in samples_dir.glob('*.ncw'):
        inventory['total_files'] += 1
        parts = ncw_file.stem.split('_')

        mic = parts[0]
        inventory['mic_positions'].add(mic)

        # Find dyn and rr indices
        dyn_idx = next((i for i, p in enumerate(parts) if p.startswith('dyn')), None)
        rr_idx = next((i for i, p in enumerate(parts) if p.startswith('rr')), None)

        if dyn_idx is None or rr_idx is None:
            continue

        drum_name = '_'.join(parts[1:dyn_idx])
        dyn_num = int(parts[dyn_idx][3:])
        # Handle filenames like "rr5 L" (strip non-digit suffix)
        rr_str = ''.join(c for c in parts[rr_idx][2:] if c.isdigit())
        rr_num = int(rr_str) if rr_str else 1

        if drum_name not in inventory['drums']:
            inventory['drums'][drum_name] = {'max_dyn': 0, 'max_rr': 0, 'mics': set()}

        inventory['drums'][drum_name]['max_dyn'] = max(
            inventory['drums'][drum_name]['max_dyn'], dyn_num)
        inventory['drums'][drum_name]['max_rr'] = max(
            inventory['drums'][drum_name]['max_rr'], rr_num)
        inventory['drums'][drum_name]['mics'].add(mic)

    # Convert sets to lists for JSON serialization
    inventory['mic_positions'] = sorted(inventory['mic_positions'])
    for drum in inventory['drums'].values():
        drum['mics'] = sorted(drum['mics'])

    return inventory


def main():
    parser = argparse.ArgumentParser(description='Batch convert NCW files to WAV')
    parser.add_argument('input_dir', type=Path, help='Directory containing NCW files')
    parser.add_argument('output_dir', type=Path, help='Output directory for WAV files')
    parser.add_argument('--workers', type=int, default=os.cpu_count(),
                        help='Number of parallel workers')
    parser.add_argument('--inventory-only', action='store_true',
                        help='Only build inventory, skip conversion')
    args = parser.parse_args()

    # Build inventory first
    print("Building sample inventory...")
    inventory = build_inventory(args.input_dir)
    print(f"  Total files: {inventory['total_files']}")
    print(f"  Mic positions: {inventory['mic_positions']}")
    print(f"  Unique drums/articulations: {len(inventory['drums'])}")

    # Save inventory
    args.output_dir.mkdir(parents=True, exist_ok=True)
    inv_path = args.output_dir / 'sample_inventory.json'
    with open(inv_path, 'w') as f:
        json.dump(inventory, f, indent=2)
    print(f"  Inventory saved to: {inv_path}")

    if args.inventory_only:
        # Print drum summary
        print("\nDrum articulations:")
        for drum, info in sorted(inventory['drums'].items()):
            print(f"  {drum}: {info['max_dyn']} velocity layers, "
                  f"{info['max_rr']} round-robins, {len(info['mics'])} mics")
        return

    # Batch convert
    ncw_files = list(args.input_dir.glob('*.ncw'))
    wav_dir = args.output_dir / 'wav'
    wav_dir.mkdir(parents=True, exist_ok=True)

    print(f"\nConverting {len(ncw_files)} NCW files to WAV...")
    print(f"  Workers: {args.workers}")
    print(f"  Output: {wav_dir}")

    start = time.time()
    success_count = 0
    fail_count = 0
    skip_count = 0

    with ProcessPoolExecutor(max_workers=args.workers) as executor:
        futures = {
            executor.submit(convert_single, str(f), str(wav_dir)): f
            for f in ncw_files
        }

        for i, future in enumerate(as_completed(futures), 1):
            name, success, msg = future.result()
            if success:
                if 'skip' in msg:
                    skip_count += 1
                else:
                    success_count += 1
            else:
                fail_count += 1
                print(f"  FAIL: {name}: {msg}")

            if i % 100 == 0:
                elapsed = time.time() - start
                rate = i / elapsed
                remaining = (len(ncw_files) - i) / rate
                print(f"  Progress: {i}/{len(ncw_files)} "
                      f"({rate:.1f} files/sec, ~{remaining:.0f}s remaining)")

    elapsed = time.time() - start
    print(f"\nDone in {elapsed:.1f}s")
    print(f"  Converted: {success_count}")
    print(f"  Skipped: {skip_count}")
    print(f"  Failed: {fail_count}")


if __name__ == '__main__':
    main()
