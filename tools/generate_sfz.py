#!/usr/bin/env python3
"""
Generate SFZ instrument files from the MIDI mapping and converted WAV samples.
Creates one SFZ per mic position + a combined full-kit SFZ.
"""

import json
from pathlib import Path
import argparse


def velocity_range(dyn_layer: int, total_layers: int) -> tuple[int, int]:
    """Calculate velocity range for a given dynamic layer (1-based)."""
    step = 127 / total_layers
    lo = int((dyn_layer - 1) * step) + 1
    hi = int(dyn_layer * step) if dyn_layer < total_layers else 127
    if dyn_layer == 1:
        lo = 1
    return lo, hi


def generate_sfz_for_mic(midi_map: dict, mic: str, wav_dir: Path) -> str:
    """Generate SFZ content for a single mic position."""
    lines = [
        f"// GGD Matt Halpern Signature Pack - {mic}",
        f"// Auto-generated SFZ mapping",
        "",
        "<control>",
        f"default_path={mic}/",
        "",
    ]

    for note_str, entry in sorted(midi_map.items(), key=lambda x: int(x[0])):
        midi_note = int(note_str)
        name = entry['name']

        for sample_info in entry['samples']:
            prefix = sample_info['sample_prefix']
            n_dyn = sample_info['velocity_layers']
            n_rr = sample_info['round_robins']
            mics = sample_info['mic_positions']

            if mic not in mics:
                continue

            lines.append(f"// --- {name} ({prefix}) ---")
            lines.append(f"// {n_dyn} velocity layers, {n_rr} round robins")
            lines.append("")

            for dyn in range(1, n_dyn + 1):
                lovel, hivel = velocity_range(dyn, n_dyn)

                for rr in range(1, n_rr + 1):
                    filename = f"{mic}_{prefix}_dyn{dyn}_rr{rr}.wav"

                    # Check if file would exist
                    lines.append(f"<region>")
                    lines.append(f"sample={filename}")
                    lines.append(f"key={midi_note}")
                    lines.append(f"lovel={lovel} hivel={hivel}")
                    lines.append(f"seq_length={n_rr} seq_position={rr}")
                    lines.append("")

    return "\n".join(lines)


def generate_combined_sfz(midi_map: dict, mic_positions: list) -> str:
    """Generate a combined SFZ with all mics as groups (for multi-output or mixing)."""
    lines = [
        "// GGD Matt Halpern Signature Pack - Full Kit (All Mics)",
        "// Auto-generated SFZ mapping",
        "// Each mic position is a separate group for mixing",
        "",
        "<control>",
        "default_path=wav/",
        "",
    ]

    for mic_idx, mic in enumerate(mic_positions):
        lines.append(f"// ===== {mic} (Group {mic_idx + 1}) =====")
        lines.append(f"<master>")
        lines.append(f"group_label={mic}")
        lines.append("")

        for note_str, entry in sorted(midi_map.items(), key=lambda x: int(x[0])):
            midi_note = int(note_str)

            for sample_info in entry['samples']:
                prefix = sample_info['sample_prefix']
                n_dyn = sample_info['velocity_layers']
                n_rr = sample_info['round_robins']
                mics = sample_info['mic_positions']

                if mic not in mics:
                    continue

                for dyn in range(1, n_dyn + 1):
                    lovel, hivel = velocity_range(dyn, n_dyn)

                    for rr in range(1, n_rr + 1):
                        filename = f"{mic}/{mic}_{prefix}_dyn{dyn}_rr{rr}.wav"
                        lines.append(f"<region>")
                        lines.append(f"sample={filename}")
                        lines.append(f"key={midi_note}")
                        lines.append(f"lovel={lovel} hivel={hivel}")
                        lines.append(f"seq_length={n_rr} seq_position={rr}")
                        lines.append("")

    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description='Generate SFZ files from MIDI mapping')
    parser.add_argument('--mapping', type=Path, default=Path('output/midi_map.json'))
    parser.add_argument('--inventory', type=Path, default=Path('output/sample_inventory.json'))
    parser.add_argument('--output-dir', type=Path, default=Path('output/sfz'))
    parser.add_argument('--wav-dir', type=Path, default=Path('output/wav'),
                        help='Directory containing converted WAV files')
    args = parser.parse_args()

    with open(args.mapping) as f:
        midi_map = json.load(f)
    with open(args.inventory) as f:
        inventory = json.load(f)

    mic_positions = inventory['mic_positions']
    args.output_dir.mkdir(parents=True, exist_ok=True)

    # Generate per-mic SFZ files
    for mic in mic_positions:
        sfz_content = generate_sfz_for_mic(midi_map, mic, args.wav_dir)
        sfz_path = args.output_dir / f"Halpern_{mic}.sfz"
        with open(sfz_path, 'w') as f:
            f.write(sfz_content)
        print(f"Generated: {sfz_path}")

    # Generate combined SFZ
    combined = generate_combined_sfz(midi_map, mic_positions)
    combined_path = args.output_dir / "Halpern_FullKit.sfz"
    with open(combined_path, 'w') as f:
        f.write(combined)
    print(f"Generated: {combined_path}")

    print(f"\nDone! {len(mic_positions) + 1} SFZ files written to {args.output_dir}")
    print(f"To use: load any .sfz file in sforzando or another SFZ-compatible sampler")
    print(f"Note: WAV files must be converted first (run batch_convert.py)")


if __name__ == '__main__':
    main()
