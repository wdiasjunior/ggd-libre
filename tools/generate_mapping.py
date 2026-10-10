#!/usr/bin/env python3
"""
Generate the MIDI mapping for GGD Matt Halpern Signature Pack.
Maps MIDI notes -> drum articulations -> sample filenames.

Source: GGD published MIDI map (HalpernOriginalDefault.txt) + sample inventory.
"""

import json
from pathlib import Path

# From HalpernOriginalDefault.txt - MIDI note -> articulation name
MIDI_MAP_RAW = {
    24: "Kick Main Hit",
    25: "Stick Click Kick Hit",
    26: "Snare Hit",
    27: "Flam",
    28: "Ruff",
    29: "Snare-Off",
    30: "Stick Click",
    33: "Hi Tom Hit",
    34: "Hi Tom Rim Hit",
    35: "Mid Tom 1 Main Hit",
    36: "Mid Tom 1 Rim Hit",
    37: "Mid Tom 2 Main Hit",
    38: "Mid Tom 2 Rim Hit",
    39: "Floor Tom Main Hit",
    40: "Floor Tom Rim Hit",
    43: "Pedal Chik",
    44: "Pedal Ching",
    45: "Tip Tight",
    46: "Edge Tight",
    47: "Tip Closed",
    48: "Edge Closed",
    49: "Tip Loose",
    50: "Edge Loose",
    51: "Tip Open 1",
    52: "Edge Open 1",
    53: "Tip Open 2",
    54: "Edge Open 2",
    55: "Tip Open 3",
    56: "Edge Open 3",
    57: "Tip Wide",
    58: "Edge Wide",
    62: "Left Crash Hit",
    63: "Left Crash Bell",
    64: "Left Crash Choke",
    65: "Left Crash Swell",
    67: "Right Crash Hit",
    68: "Right Crash Bell",
    69: "Right Crash Choke",
    70: "Right Crash Swell",
    72: "Ride Tip",
    73: "Ride Crash",
    74: "Ride Bell Tip",
    75: "Ride Bell Shoulder",
    76: "China Main Hit",
    77: "China Choke",
    81: "Stack Tight Hit",
    82: "Stack Loose Hit",
    83: "Splash Hit",
}

# Map from MIDI map articulation names -> sample filename patterns
# This connects the published map to what's actually in the sample files
ARTICULATION_TO_FILENAME = {
    # Kicks
    "Kick Main Hit": ["Kick22x16_MainHit", "Kick22x20_MainHit", "Kick22x16", "Kick22x20"],
    "Stick Click Kick Hit": ["Kick_Click"],

    # Snare (main snare = SnareHigh based on the 10 velocity layers)
    "Snare Hit": ["SnareHigh"],
    "Flam": ["SnareHigh_Flam"],
    "Ruff": ["SnareHigh_Ruff"],
    "Snare-Off": ["SnareHigh_Off"],
    "Stick Click": ["SnareHigh_click"],

    # Toms (ClearTom = default kit, CoatedTom = alternate)
    "Hi Tom Hit": ["ClearTom10", "CoatedTom10"],
    "Hi Tom Rim Hit": ["ClearTom10_Rim"],
    "Mid Tom 1 Main Hit": ["ClearTom12", "CoatedTom12"],
    "Mid Tom 1 Rim Hit": ["ClearTom12_rim"],
    "Mid Tom 2 Main Hit": ["ClearTom14", "CoatedTom14"],
    "Mid Tom 2 Rim Hit": ["ClearTom14_rim"],
    "Floor Tom Main Hit": ["ClearTom16", "CoatedTom16"],
    "Floor Tom Rim Hit": ["ClearTom16_rim"],

    # Hi-Hat (Tip = HiHat_Tip*, Edge = HiHat_Shoulder*)
    "Pedal Chik": ["HiHat_chik"],
    "Pedal Ching": ["HiHat_ching"],
    "Tip Tight": ["HiHat_TightTip"],
    "Edge Tight": ["HiHat_ShoulderTight"],
    "Tip Closed": ["HiHat_TipClosed"],
    "Edge Closed": ["HiHat_ShoulderClosed"],
    "Tip Loose": ["HiHat_TipMedClosed"],
    "Edge Loose": ["HiHat_ShoulderMedClosed"],
    "Tip Open 1": ["HiHat_TipMed"],
    "Edge Open 1": ["HiHat_ShoulderMed"],
    "Tip Open 2": ["HiHat_TipMedOpen"],
    "Edge Open 2": ["HiHat_ShoulderMedOpen"],
    "Tip Open 3": ["HiHat_TipOpen"],
    "Edge Open 3": ["HiHat_ShoulderOpen"],
    "Tip Wide": ["HiHat_TipWide"],
    "Edge Wide": ["HiHat_ShoulderWide"],

    # Crashes (Left = 17ByzThinCrash, Right = 20ByzThinCrash based on typical GGD layout)
    "Left Crash Hit": ["17ByzThinCrash_MainHit"],
    "Left Crash Bell": ["17ByzThinCrash_Bell"],
    "Left Crash Choke": ["17ByzThinCrash_Choke"],
    "Left Crash Swell": ["17ByzThinCrash_Swell"],
    "Right Crash Hit": ["20ByzThinCrash_MainHit"],
    "Right Crash Bell": ["20ByzThinCrash_Bell"],
    "Right Crash Choke": ["20ByzThinCrash_Choke"],
    "Right Crash Swell": ["20ByzThinCrash_Swell"],

    # Ride
    "Ride Tip": ["Ride_Tip"],
    "Ride Crash": ["Ride_Crash"],
    "Ride Bell Tip": ["Ride_BellTip"],
    "Ride Bell Shoulder": ["Ride_BellShoulder"],

    # China
    "China Main Hit": ["China_MainHit", "18China_MainHit"],
    "China Choke": ["China_Choke", "18China_Choke"],

    # Stacks & Splash
    "Stack Tight Hit": ["StackTight_MainHit", "MiniStack_Tight"],
    "Stack Loose Hit": ["StackLoose_MainHit", "MiniStack_Loose"],
    "Splash Hit": ["Splash_MainHit"],
}


def generate_mapping(inventory_path: Path, output_path: Path):
    """Generate the full MIDI mapping JSON."""
    with open(inventory_path) as f:
        inventory = json.load(f)

    available_drums = set(inventory['drums'].keys())
    mic_positions = inventory['mic_positions']

    mapping = {}
    unmatched_artics = []

    for midi_note, artic_name in MIDI_MAP_RAW.items():
        filename_patterns = ARTICULATION_TO_FILENAME.get(artic_name, [])

        # Find matching drums in inventory
        matched = []
        for pattern in filename_patterns:
            if pattern in available_drums:
                drum_info = inventory['drums'][pattern]
                matched.append({
                    'sample_prefix': pattern,
                    'velocity_layers': drum_info['max_dyn'],
                    'round_robins': drum_info['max_rr'],
                    'mic_positions': drum_info['mics'],
                })

        if not matched:
            unmatched_artics.append((midi_note, artic_name, filename_patterns))

        mapping[str(midi_note)] = {
            'name': artic_name,
            'midi_note': midi_note,
            'samples': matched,
        }

    # Report unmatched
    if unmatched_artics:
        print(f"\nWARNING: {len(unmatched_artics)} articulations could not be matched:")
        for note, name, patterns in unmatched_artics:
            print(f"  MIDI {note}: '{name}' (tried: {patterns})")
        print(f"\nAvailable drums not yet mapped:")
        mapped_prefixes = set()
        for entry in mapping.values():
            for s in entry['samples']:
                mapped_prefixes.add(s['sample_prefix'])
        unmapped = available_drums - mapped_prefixes
        for d in sorted(unmapped):
            info = inventory['drums'][d]
            print(f"  {d} ({info['max_dyn']} dyn, {info['max_rr']} rr)")

    # Save
    output_path.parent.mkdir(parents=True, exist_ok=True)
    with open(output_path, 'w') as f:
        json.dump(mapping, f, indent=2)

    print(f"\nMIDI mapping saved to: {output_path}")
    print(f"  Mapped: {sum(1 for v in mapping.values() if v['samples'])}/{len(mapping)} articulations")

    return mapping


if __name__ == '__main__':
    inventory_path = Path('output/halpern/sample_inventory.json')
    output_path = Path('output/halpern/midi_map.json')

    if not inventory_path.exists():
        print("Run batch_convert.py --inventory-only first to generate sample_inventory.json")
        exit(1)

    generate_mapping(inventory_path, output_path)
