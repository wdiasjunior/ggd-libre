#!/usr/bin/env python3
"""
Build library_index.json for the ggd-libre plugin.

The plugin no longer parses sample filenames itself. This script turns an
extracted library (a wav/ tree) into an index the plugin loads directly:

  {
    "format": 1,
    "library": "<slug>",
    "mics": ["<mic id>", ...],
    "articulations": {"<art id>": {"<mic id>": [[rr paths...] per layer]}},
    "notes": {"<note>": {"name": "...", "play": ["<art id>", ...]}},
    "selectors": {"<key>": [{"<note>": ["<art id>", ...]} per option]}
  }

Paths are relative to <library>/wav/ and null marks a missing sample.
"play" is what a note triggers when no selector governs it. A selector option
replaces the art list of every note it names. Selector keys and option counts
must match the plugin's library descriptor (plugin/src/lib_*.c).

All three libraries share the Halpern note layout (24-83) so one MIDI part
plays any kit.

Usage:
  python build_index.py halpern    ../output/halpern
  python build_index.py okw_metal  ../output/okw_metal
  python build_index.py pv_halpern ../output/pv_halpern
"""

import argparse
import json
import re
import sys
from collections import defaultdict
from pathlib import Path

# ---------------------------------------------------------------------------
# Halpern (GGD Matt Halpern Signature Pack)
# ---------------------------------------------------------------------------

HALPERN_MICS = ["CloseMic", "OHMic", "NearRoomMic", "FarRoomMic",
                "BottomMic", "TopMic1", "TopMic2"]
# Same limits the old in-plugin filename scanner applied.
HALPERN_MAX_LAYERS = 10
HALPERN_MAX_RR = 7

HALPERN_SNARE_PREFIXES = [
    ["SnareHigh", "SnareHigh_Flam", "SnareHigh_Ruff", "SnareHigh_Off", "SnareHigh_click"],
    ["SnareMed", "SnareMed_Flam", "SnareMed_Ruff", "SnareMed_Off", "SnareMed_Stick"],
    ["SnareLow", "SnareLow_Flam", "SnareLow_Ruff", "SnareLow_Off", "SnareLow_Stick"],
    ["13Snare", "13Snare_Flam", "13Snare_Ruff", "13Snare_off", "13Snare_Stick"],
    ["SnareBFSD", "SnareBFSD_Flam", None, None, "SnareBFSD_Stick"],
]
HALPERN_LCRASH_PREFIXES = [
    ["17ByzThinCrash_MainHit", "17ByzThinCrash_Bell", "17ByzThinCrash_Choke", "17ByzThinCrash_Swell"],
    ["18MedByzThinCrash", "18MedByzThinCrash_Bell", "18MedByzThinCrash_Choke", "18MedByzThinCrash_Swell"],
]
HALPERN_RCRASH_PREFIXES = [
    ["20ByzThinCrash_MainHit", "20ByzThinCrash_Bell", "20ByzThinCrash_Choke", "20ByzThinCrash_Swell"],
    ["19MedByzThinCrash", "19MedByzThinCrash_bell", "19MedByzThinCrash_Choke", "19MedByzThinCrash_Swell"],
]


def build_halpern(root: Path) -> dict:
    wav = root / "wav"
    # Mirror the old scanner: <Mic>_<prefix>_dyn<N>_rr<N>.wav, prefix = text up
    # to the first "_dyn".
    arts: dict = defaultdict(dict)
    pat = re.compile(r"_dyn(\d+)_rr(\d+)")
    for mic in HALPERN_MICS:
        mic_dir = wav / mic
        if not mic_dir.is_dir():
            continue
        for f in sorted(mic_dir.iterdir()):
            name = f.name
            if not name.endswith(".wav") or not name.startswith(mic + "_"):
                continue
            rest = name[len(mic) + 1:]
            i = rest.find("_dyn")
            if i < 0:
                continue
            m = pat.match(rest, i)
            if not m:
                continue
            prefix, dyn, rr = rest[:i], int(m.group(1)), int(m.group(2))
            if not (1 <= dyn <= HALPERN_MAX_LAYERS and 1 <= rr <= HALPERN_MAX_RR):
                continue
            grid = arts[prefix].setdefault(mic, {})
            grid[(dyn, rr)] = f"{mic}/{name}"

    articulations = {a: {mic: to_layers(g) for mic, g in mics.items()}
                     for a, mics in arts.items()}

    midi_map = json.loads((root / "midi_map.json").read_text())
    notes = {}
    variants = {}
    for key, entry in midi_map.items():
        vs = [s["sample_prefix"] for s in entry.get("samples", [])
              if s["sample_prefix"] in articulations]
        if not vs:
            continue
        variants[int(key)] = vs
        notes[key] = {"name": entry["name"], "play": vs[:1]}

    def has(p):
        return p is not None and p in articulations

    def swap_table(prefix_table, note_list):
        # Option o replaces the note's first variant. Missing prefixes fall back
        # to option 0, so every option is a complete, deterministic mapping.
        opts = []
        for row in prefix_table:
            opt = {}
            for i, note in enumerate(note_list):
                if note not in variants:
                    continue
                p = row[i] if has(row[i]) else prefix_table[0][i]
                if not has(p):
                    p = variants[note][0]
                opt[str(note)] = [p]
            opts.append(opt)
        return opts

    def pick_variant(note_list, n_opts):
        opts = []
        for o in range(n_opts):
            opt = {}
            for note in note_list:
                vs = variants.get(note)
                if vs and len(vs) > 1:
                    opt[str(note)] = [vs[o] if o < len(vs) else vs[0]]
            opts.append(opt)
        return opts

    # Kick: 4 variants, [close16, close20, room16, room20]; play close + room of
    # the chosen size.
    kick = []
    for size in range(2):
        vs = variants.get(24, [])
        kick.append({"24": [v for i, v in enumerate(vs) if i % 2 == size] or vs[:1]})

    selectors = {
        "kick_size": kick,
        "snare_type": swap_table(HALPERN_SNARE_PREFIXES, [26, 27, 28, 29, 30]),
        "tom1_head": pick_variant([33], 2),
        "tom2_head": pick_variant([35], 2),
        "tom3_head": pick_variant([37], 2),
        "tom4_head": pick_variant([39], 2),
        "china": pick_variant([76, 77], 2),
        "stack": pick_variant([81, 82], 2),
        "lcrash": swap_table(HALPERN_LCRASH_PREFIXES, [62, 63, 64, 65]),
        "rcrash": swap_table(HALPERN_RCRASH_PREFIXES, [67, 68, 69, 70]),
    }
    return {"mics": HALPERN_MICS, "articulations": articulations,
            "notes": notes, "selectors": selectors}


# ---------------------------------------------------------------------------
# GGD third-generation naming (One Kit Wonder Metal, PV Matt Halpern)
#   GGD-<LIB>-<Piece>-<qualifiers...>-<Artic>-<MicTok>[-<Model>]-RR<n>-V<n>.wav
# ---------------------------------------------------------------------------

GGD3_MIC_TOKENS = {
    # token -> mic id used by the plugin descriptor
    "Cls": "Cls", "OH": "OH",
    "Rm": "Rm", "RmMS": "Rm",                     # OKW room (Mid/Side pair)
    "KckIn": "KckIn", "KckOut": "KckOut", "KckPrt": "KckPrt",
    "SnrTop": "SnrTop", "SnrBtm": "SnrBtm",
    "RmClsFOK": "RmCls", "RmFarBlm": "RmFarBlm", "RmFarWde": "RmFarWde",
}


def parse_ggd3(stem: str, prefix: str):
    if not stem.startswith(prefix):
        return None
    t = stem[len(prefix):].split("-")
    if len(t) < 4 or not re.fullmatch(r"RR\d+", t[-2]) or not re.fullmatch(r"V\d+", t[-1]):
        return None
    rr, vel = int(t[-2][2:]), int(t[-1][1:])
    t = t[:-2]
    mic_idx = [i for i, tok in enumerate(t) if tok in GGD3_MIC_TOKENS]
    if not mic_idx:
        return None
    mi = mic_idx[-1]
    return "-".join(t[:mi]), GGD3_MIC_TOKENS[t[mi]], vel, rr


def scan_ggd3(root: Path, prefix: str, merge: dict | None = None,
              stack: dict | None = None) -> dict:
    """Return {art: {mic: {(layer, rr): path}}} with dense 1-based numbering.

    merge maps source art ids onto one target art id; their round robins are
    concatenated in the order given (used for OKW's alternating kick feet).
    stack builds a new art from several whose velocity numbers continue each
    other (OKW: center hits V1-V14, rimshots V15-V18); the sources stay too.
    """
    raw: dict = defaultdict(lambda: defaultdict(dict))
    for f in sorted((root / "wav").rglob("*.wav")):
        p = parse_ggd3(f.stem, prefix)
        if not p:
            continue
        art, mic, vel, rr = p
        raw[art][mic][(vel, rr)] = str(f.relative_to(root / "wav"))

    merge = merge or {}
    out: dict = defaultdict(dict)
    targets = defaultdict(list)
    for src, dst in merge.items():
        targets[dst].append(src)
    for art, mics in raw.items():
        if art in merge:
            continue
        for mic, grid in mics.items():
            out[art][mic] = densify(grid)
    for dst, srcs in (stack or {}).items():
        for mic in raw[srcs[0]]:
            union = {}
            for src in srcs:
                union.update(raw[src].get(mic, {}))
            out[dst][mic] = densify(union)
    for dst, srcs in targets.items():
        for mic in raw[srcs[0]]:
            merged, rr_off = {}, 0
            for src in srcs:
                g = densify(raw[src].get(mic, {}))
                for (l, r), path in g.items():
                    merged[(l, r + rr_off)] = path
                rr_off += max((r for _, r in g), default=0)
            out[dst][mic] = merged
    return out


def densify(grid: dict) -> dict:
    """Renumber layers and round robins to 1..N (the OKW rimshot only has V15-V18)."""
    layers = {v: i + 1 for i, v in enumerate(sorted({v for v, _ in grid}))}
    rrs = {r: i + 1 for i, r in enumerate(sorted({r for _, r in grid}))}
    return {(layers[v], rrs[r]): p for (v, r), p in grid.items()}


def to_layers(grid: dict) -> list:
    if not grid:
        return []
    n_l = max(l for l, _ in grid)
    n_r = max(r for _, r in grid)
    return [[grid.get((l, r)) for r in range(1, n_r + 1)] for l in range(1, n_l + 1)]


# Unified note names (Halpern layout plus 31 = rimshot).
NOTE_NAMES = {
    24: "Kick", 25: "Stick Click", 26: "Snare Hit", 27: "Snare Flam", 28: "Snare Ruff",
    29: "Snare Wires Off", 30: "Cross Stick", 31: "Snare Rimshot",
    33: "Tom 1", 34: "Tom 1 Rim", 35: "Tom 2", 36: "Tom 2 Rim",
    37: "Tom 3", 38: "Tom 3 Rim", 39: "Tom 4", 40: "Tom 4 Rim",
    43: "Hat Pedal", 44: "Hat Pedal Splash", 45: "Hat Tight Tip", 46: "Hat Tight Edge",
    47: "Hat Closed Tip", 48: "Hat Closed Edge", 49: "Hat Loose Tip", 50: "Hat Loose Edge",
    51: "Hat Open 1 Tip", 52: "Hat Open 1 Edge", 53: "Hat Open 2 Tip", 54: "Hat Open 2 Edge",
    55: "Hat Open 3 Tip", 56: "Hat Open 3 Edge", 57: "Hat Wide Tip", 58: "Hat Wide Edge",
    62: "L Crash Hit", 63: "L Crash Bell", 64: "L Crash Choke", 65: "L Crash Swell",
    67: "R Crash Hit", 68: "R Crash Bell", 69: "R Crash Choke", 70: "R Crash Swell",
    72: "Ride Bow", 73: "Ride Crash", 74: "Ride Bell", 75: "Ride Bell 2",
    76: "China Hit", 77: "China Choke", 81: "Stack Tight", 82: "Stack Loose", 83: "Splash Hit",
}

HAT_NOTES = {43: "Pdl", 44: "Pdl", 45: "TghtTp", 46: "TghtEdg", 47: "ClsdTp", 48: "ClsdEdg",
             49: "Opn1", 50: "Opn1", 51: "Opn1", 52: "Opn1", 53: "Opn2", 54: "Opn2",
             55: "Opn3", 56: "Opn3", 57: "Opn3", 58: "Opn3"}


def first(arts: dict, *cands: str) -> str | None:
    for c in cands:
        if c in arts:
            return c
    return None


def crash_notes(arts: dict, piece: str, notes: tuple) -> dict:
    """Map one crash piece onto a 4-note crash block (hit, bell, choke, swell)."""
    hit = first(arts, f"{piece}-Crsh")
    bell = first(arts, f"{piece}-Bell", f"{piece}-Crsh")
    choke = first(arts, f"{piece}-Chk")
    out = {}
    for note, art in zip(notes, (hit, bell, choke, hit)):
        if art:
            out[str(note)] = [art]
    return out


def ggd3_common(arts: dict, notes: dict, kick: list, snare: dict, toms: list,
                hats: str, ride: dict, china: str | None, stack: list, splash: str | None):
    """Fill the shared note layout. Each argument names art ids for its slot."""
    def put(note, art):
        if art and art in arts:
            notes[str(note)] = {"name": NOTE_NAMES[note], "play": [art]}

    for a in kick:
        notes["24"] = {"name": NOTE_NAMES[24], "play": [a]}
        break
    for note, art in snare.items():
        put(note, art)
    for i, tom in enumerate(toms):
        put(33 + 2 * i, tom)
        put(34 + 2 * i, tom)          # no rim samples: rim notes play the hit
    for note, artic in HAT_NOTES.items():
        put(note, f"{hats}-{artic}")
    for note, art in ride.items():
        put(note, art)
    if china:
        put(76, first(arts, f"{china}-Crsh"))
        put(77, first(arts, f"{china}-Chk"))
    for note, art in zip((81, 82), stack):
        put(note, art)
    if splash:
        put(83, first(arts, f"{splash}-Crsh"))


def build_okw(root: Path) -> dict:
    kick = "22x16TamaStclMplKck-Hit"
    sn = "14x8PrlVPSigSnr-Eb"
    # The snare's velocity numbers run V1-V14 on center hits and continue
    # V15-V18 on rimshots: the hardest hits are rimshots.
    hit = f"{sn}-Hit"
    raw = scan_ggd3(root, "GGD-OKW3-", merge={
        "22x16TamaStclMplKck-HitL": kick, "22x16TamaStclMplKck-HitR": kick},
        stack={hit: [f"{sn}-HitCtr", f"{sn}-HitRim"]})
    arts = {a: {m: to_layers(g) for m, g in mics.items()} for a, mics in raw.items()}

    notes: dict = {}
    ggd3_common(
        arts, notes,
        kick=[kick],
        snare={26: hit, 27: hit, 28: hit, 29: hit, 30: f"{sn}-CrsStk", 31: f"{sn}-HitRim"},
        toms=["10x9TamaStclMplTm-D-Hit", "12x10TamaStclMplTm-A-Hit",
              "14x14TamaStclMplTm-E-Hit", "16x16TamaStclMplTm-B-Hit"],
        hats="14PstTwntCstmMtlHats",
        ride={72: "22PstTwntCstmMtlRde-Bow", 73: "22PstTwntCstmMtlRde-Bow",
              74: "22PstTwntCstmMtlRde-Bell", 75: "22PstTwntCstmMtlRde-Bell"},
        china="18PstTwntCstmThnChn",
        # No stack in this kit: the stack notes play the 10" splash.
        stack=["10Pst2002Spls-Crsh", "10Pst2002Spls-Crsh"],
        splash="8Pst2002Spls",
    )
    crashes = ["17PstF602MdrnEssCrsh", "18PstF602MdrnEssCrsh", "19PstF602MdrnEssCrsh"]
    selectors = {
        "lcrash": [crash_notes(arts, c, (62, 63, 64, 65)) for c in crashes],
        "rcrash": [crash_notes(arts, c, (67, 68, 69, 70)) for c in crashes],
        "splash": [{"83": [f"{s}-Crsh"]} for s in ("8Pst2002Spls", "10Pst2002Spls")],
    }
    # Default plays for the selector-governed notes (descriptor defaults).
    for sel, opt in (("lcrash", 1), ("rcrash", 2)):
        notes.update({n: {"name": NOTE_NAMES[int(n)], "play": a}
                      for n, a in selectors[sel][opt].items()})
    return {"mics": ["Cls", "OH", "Rm"], "articulations": arts,
            "notes": notes, "selectors": selectors}


def build_pv(root: Path) -> dict:
    raw = scan_ggd3(root, "GGD-PV-")
    arts = {a: {m: to_layers(g) for m, g in mics.items()} for a, mics in raw.items()}

    sn = "14x6PrlMHSigSnr"
    tunings = ["Low-D", "Mid-E", "Hgh-F#"]

    def snare(t):
        hit = f"{sn}-{t}-Hit"
        return {26: hit, 27: hit, 28: hit, 29: f"{sn}-{t}-WrsOff", 30: f"{sn}-{t}-CrsStk", 31: hit}

    notes: dict = {}
    ggd3_common(
        arts, notes,
        kick=["22x18PrlRefKck-Hit"],
        snare=snare("Mid-E"),
        toms=["12x8PrlRefTom-Hit", "13x9PrlRefTom-Hit", "16x16PrlRefTom-Hit", "18x16PrlRefTom-Hit"],
        hats="15MnlByzTrdMdmHats",
        ride={},
        china="20MnlByzTrdChn",
        stack=["17_18MnlMHSigDDStack-Crsh", "17_18MnlMHSigDDStack-Crsh"],
        splash="10MnlByzTradSpls",
    )
    if "StkClk-Hit" in arts:
        notes["25"] = {"name": NOTE_NAMES[25], "play": ["StkClk-Hit"]}

    crashes = ["18MnlByzFndRsrvCrsh", "18MnlByzTrdPlyCrsh", "18MnlByzTrdXThnHmrdCrsh",
               "19MnlByzFndRsrvCrsh", "19MnlByzTrdPlyCrsh", "21MnlMHSigDDCrshRde"]

    def ride(piece):
        bow, bell = f"{piece}-Bow", f"{piece}-Bell"
        crash = first(arts, f"{piece}-Crsh", bow)
        return {"72": [bow], "73": [crash], "74": [bell], "75": [bell]}

    selectors = {
        "kick": [{"24": ["22x18PrlRefKck-Hit"]}, {"24": ["22x18PrlMstrsBrchKck-Hit"]}],
        "snare_tuning": [{str(n): [a] for n, a in snare(t).items()} for t in tunings],
        "lcrash": [crash_notes(arts, c, (62, 63, 64, 65)) for c in crashes],
        "rcrash": [crash_notes(arts, c, (67, 68, 69, 70)) for c in crashes],
        "ride": [ride("21MnlMHSigDDCrshRde"), ride("15MnlRDBgBllMiniRde")],
    }
    # Default plays for the selector-governed notes (descriptor defaults).
    for sel, opt in (("lcrash", 0), ("rcrash", 3), ("ride", 0)):
        notes.update({n: {"name": NOTE_NAMES[int(n)], "play": a}
                      for n, a in selectors[sel][opt].items()})
    return {"mics": ["KckIn", "KckOut", "KckPrt", "SnrTop", "SnrBtm", "Cls", "OH",
                     "RmCls", "RmFarBlm", "RmFarWde"],
            "articulations": arts, "notes": notes, "selectors": selectors}


BUILDERS = {"halpern": build_halpern, "okw_metal": build_okw, "pv_halpern": build_pv}


def validate(index: dict) -> list:
    errs = []
    arts = index["articulations"]
    for a, mics in arts.items():
        for m in mics:
            if m not in index["mics"]:
                errs.append(f"art {a}: unknown mic {m}")
    for n, e in index["notes"].items():
        for a in e["play"]:
            if a not in arts:
                errs.append(f"note {n}: unknown art {a}")
    for key, opts in index["selectors"].items():
        for i, opt in enumerate(opts):
            for n, al in opt.items():
                for a in al:
                    if a not in arts:
                        errs.append(f"selector {key}[{i}] note {n}: unknown art {a}")
    return errs


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("library", choices=sorted(BUILDERS))
    ap.add_argument("root", type=Path, help="library folder containing wav/")
    args = ap.parse_args()

    index = BUILDERS[args.library](args.root)
    errs = validate(index)
    for e in errs:
        print("ERROR:", e, file=sys.stderr)
    if errs:
        sys.exit(1)

    out = {"format": 1, "library": args.library, **index}
    path = args.root / "library_index.json"
    path.write_text(json.dumps(out, separators=(",", ":")))
    n_files = sum(1 for a in index["articulations"].values() for m in a.values()
                  for layer in m for p in layer if p)
    print(f"{path}: {len(index['articulations'])} articulations, {n_files} samples, "
          f"{len(index['notes'])} notes, {len(index['selectors'])} selectors")


if __name__ == "__main__":
    main()
