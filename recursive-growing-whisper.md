# GGD Libre — CLAP Drum Sampler Plugin

## Context

The GGD Matt Halpern Signature Pack is a Kontakt drum plugin with 8,608 samples across 85 articulations, 7 mic positions, up to 10 velocity layers, and 7 round robins. The samples have already been extracted to WAV (24-bit mono PCM, 48kHz, 8.5GB total) and mapped via `midi_map.json`. We're building a standalone CLAP plugin in C to replace Kontakt entirely, with a Cairo/X11 GUI matching the original's tab-based mixer layout.

---

## File Structure

```
vst/
├── Makefile
├── src/
│   ├── types.h              — Shared enums, constants, limits
│   ├── wav_reader.c/.h      — Minimal 24-bit PCM WAV parser → float32
│   ├── sample_bank.c/.h     — Load all WAV samples, organize by articulation/mic/vel/rr
│   ├── midi_map.c/.h        — Parse midi_map.json, build note→sample lookup, choke groups
│   ├── audio_engine.c/.h    — Voice pool, sample playback, per-channel mixing
│   ├── params.c/.h          — CLAP parameter definitions (93 params), get/set/info
│   ├── state.c/.h           — Binary state save/load
│   ├── gui.c/.h             — X11 window + Cairo rendering + CLAP GUI extension
│   ├── gui_widgets.c/.h     — Fader, knob, button, selector, tab drawing
│   ├── plugin.c/.h          — Plugin struct, lifecycle, process(), extensions
│   └── plugin_entry.c       — clap_entry, factory, descriptor
├── clap-sdk/                — (existing submodule)
└── third_party/cjson/       — (existing)
```

---

## Core Data Model

### Drum Channels (14 total)
| Tab | Channels |
|---|---|
| Kick | Kick |
| Snare | Snare |
| Toms | Rack 1 (10"), Rack 2 (12"), Floor 1 (14"), Floor 2 (16") |
| Cymbals | Hi-Hat, L Crash, R Crash, Ride, China, Stack, Splash, Accent |

### Per-Channel Controls (6 each)
Gain (dB fader, -inf to +12), Pan (-1 L to +1 R), Mute (toggle), Solo (toggle), Phase Invert (toggle), Stereo/Mono (toggle)

### Variant Selectors (8 total)
- Kick size: 22x16 / 22x20
- Snare type: High / Med / Low / 13" / BFSD
- Tom head ×4: Clear / Coated
- China size: default / 18"
- Stack type: default / Mini

### Parameter ID Scheme (93 total)
- `0` = master gain
- `channel*10 + 100 + offset` = per-channel (offset: 0=gain, 1=pan, 2=mute, 3=solo, 4=phase, 5=stereo)
- `300-307` = variant selectors

---

## Audio Engine

### Sample Loading
- Load all WAV files from `output/wav/{MicPosition}/` at plugin init
- Parse filenames: `{MicPosition}_{DrumName}_dyn{N}_rr{M}.wav`
- Organize into: `ArticulationSamples[mic][velocity_layer][round_robin]` → float* buffers
- Parse `midi_map.json` with cJSON to build MIDI note → articulation mapping
- ~8.5GB on disk → ~11GB as float32 in memory (acceptable for pro drum plugin)

### Voice Pool (64 max voices)
- Note-on: look up MIDI note → active variant → articulation, select velocity layer + next round-robin, allocate voice
- Playback: one-shot (play to end, no sustain/release envelope needed)
- Per-frame: sum all mic positions for voice's articulation, apply channel gain/pan/phase/mute/solo
- Choke: fast 5ms fade-out when choke group triggered

### Choke Groups
| Group | Choked by | Members choked |
|---|---|---|
| Hi-Hat | Chik (43), Ching (44), Tight/Closed notes | Open/Wide hi-hat notes (51-58) |
| L Crash | L Crash Choke (64) | L Crash Hit/Bell/Swell (62,63,65) |
| R Crash | R Crash Choke (69) | R Crash Hit/Bell/Swell (67,68,70) |
| China | China Choke (77) | China Hit (76) |

### Velocity Layer Selection
Same algorithm as existing `generate_sfz.py`:
```
step = 127 / num_layers
layer = clamp((midi_velocity - 1) / step, 0, num_layers - 1)
```

### Mic Mixing
Sum all available mic positions with equal gain for MVP. Samples are mono; stereo output uses pan positioning. "Stereo mode" toggle uses OH L/R panning to create width.

---

## GUI (Cairo + X11)

### Window: ~800×500 fixed size
- **Tab bar** (top): KICK | SNARE | TOMS | CYMBALS — always visible
- **Logo area** (top-left): GGD logo + drum illustration
- **Channel strips** (center): vertical faders + pan knobs + button row per drum piece
- **Variant selector** (bottom-left): dropdown/buttons for size/type selection
- **Master fader** (right): always visible

### CLAP GUI Integration
- `CLAP_WINDOW_API_X11` for embedded window
- `CLAP_EXT_TIMER_SUPPORT` for 30Hz redraw timer
- `CLAP_EXT_POSIX_FD_SUPPORT` for X11 event fd
- Parameter changes from GUI → `host->request_flush()` → CLAP param events

### Widgets
- Vertical fader (gain): click-drag, value label
- Pan knob: circular, click-drag rotation
- Toggle buttons: Mute (M), Solo (S), Phase (Φ), Stereo/Mono (S/M)
- Tab buttons: highlighted active tab
- Dropdown selector: variant selection

---

## Build

```makefile
CC = gcc
CFLAGS = -Wall -O2 -fPIC -Iclap-sdk/include -Ithird_party/cjson -Isrc
LDFLAGS = -shared -lm -lcairo -lX11
TARGET = ggd-libre.clap

install: $(TARGET)
	mkdir -p ~/.clap && cp $< ~/.clap/
```

Output is a `.clap` file (shared library with `clap_entry` symbol). Install to `~/.clap/`.

---

## Implementation Order

### Phase 1 — Audio engine (no GUI, testable standalone)
1. `types.h` — enums, constants, struct definitions
2. `wav_reader.c/.h` — parse RIFF/WAV, 24-bit PCM → float32
3. `sample_bank.c/.h` — scan wav dirs, load all samples, organize by articulation
4. `midi_map.c/.h` — parse JSON, build note slots with variants, choke group tables
5. `audio_engine.c/.h` — voice pool, note-on/off, per-frame mixing with channel controls
6. `params.c/.h` — 93 parameter definitions, info/get/set, value↔text

### Phase 2 — CLAP plugin shell
7. `plugin.c/.h` — plugin struct, init (load samples), activate, process(), extensions
8. `plugin_entry.c` — clap_entry, factory, descriptor
9. `state.c/.h` — binary state save/load
10. `Makefile` — build and install
11. **Test**: load in a CLAP host (REAPER, Bitwig), verify MIDI triggers sounds

### Phase 3 — GUI
12. `gui.c/.h` — X11 window creation/embedding, Cairo surface, timer/fd registration
13. `gui_widgets.c/.h` — fader, knob, button, selector, tab drawing functions
14. Connect GUI ↔ parameters, mouse interaction, tab switching
15. 4 tab layouts matching original screenshots

### Phase 4 — Polish
16. Choke fade-out (anti-click)
17. Solo logic (global: if any soloed, only soloed channels output)
18. Variant switching (swap active sample set, don't interrupt playing voices)
19. Error handling for missing files

---

## Verification Plan
1. **Build**: `make` succeeds, produces `ggd-libre.clap`
2. **Host load**: copy to `~/.clap/`, open in REAPER/Bitwig, plugin appears in instrument list
3. **MIDI test**: send MIDI notes 24-83 from a MIDI keyboard or DAW piano roll, verify audio output
4. **Velocity**: play same note at different velocities, confirm different sample layers trigger
5. **Round-robin**: play same note repeatedly at same velocity, confirm cycling (no machine-gun effect)
6. **Choke**: play open hi-hat then pedal chik, confirm open hat stops
7. **GUI**: verify tabs switch, faders respond to drag, mute/solo work, variant selectors change sounds
8. **State**: save DAW project, reload, verify all parameters restored

---

## Key Files Referenced
- `output/midi_map.json` — MIDI mapping source
- `output/sample_inventory.json` — sample metadata
- `output/wav/` — 8,608 WAV files across 7 mic position subdirs
- `vst/clap-sdk/src/plugin-template.c` — CLAP plugin reference implementation
- `vst/clap-sdk/include/clap/` — CLAP API headers
- `vst/third_party/cjson/cJSON.h` — JSON parser
