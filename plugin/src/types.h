#ifndef GGD_TYPES_H
#define GGD_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define MAX_LIB_MICS         16   // mic positions per library
#define MAX_LIB_CHANNELS     32   // mixer channels (= stem ports) per library
#define MAX_VOICES           256
#define MAX_MIDI_NOTES       128
#define MAX_ARTICULATIONS    128
#define MAX_VARIANTS         8

#define CHOKE_FADE_MS        5.0f   // fade applied when a choke group is triggered
#define STEAL_FADE_MS        3.0f   // fade applied to a voice before it is retriggered
#define MIC_TAIL_FADE_MS     8.0f   // ramp each mic out at its own end of file

// Total intra-layer gain tilt, centred on the recorded level (+-half this).
// The recorded velocity layers already carry the dynamics, so velocity must not
// scale volume again; this only smooths the step at layer boundaries.
#define VEL_TILT_DB          3.0f

// Fixed headroom trim. A single hit sums several mic streams at unity, so the
// bus needs room. Applied identically to every output port so the stems still
// sum to the master.
#define OUTPUT_TRIM_DB      -6.0f

// Drum types for MIDI note classification and mic routing
typedef enum {
    DRUM_KICK = 0,
    DRUM_SNARE,
    DRUM_RACK1,
    DRUM_RACK2,
    DRUM_FLOOR1,
    DRUM_FLOOR2,
    DRUM_HIHAT,
    DRUM_LCRASH,
    DRUM_RCRASH,
    DRUM_RIDE,
    DRUM_CHINA,
    DRUM_STACK,
    DRUM_SPLASH,
    DRUM_ACCENT,
    DRUM_TYPE_COUNT
} DrumType;

// One mixer channel's settings, with cached linear values.
typedef struct {
    float gain_db;
    float pan;        // -1 L, 0 center, +1 R
    bool  mute;
    bool  solo;
    bool  phase_invert;
    bool  stereo_mode; // true: keep stereo sources in stereo; false: fold to mono
    // Cached linear values
    float gain_linear;
    float pan_l, pan_r; // equal-power pan, for mono sources
    float bal_l, bal_r; // balance law (unity at centre), for stereo sources
} ChannelParams;

typedef enum {
    CHOKE_NONE = 0,
    CHOKE_HIHAT,
    CHOKE_LCRASH,
    CHOKE_RCRASH,
    CHOKE_CHINA,
    CHOKE_GROUP_COUNT
} ChokeGroup;

typedef enum {
    TAB_KICK = 0,
    TAB_SNARE,
    TAB_TOMS,
    TAB_CYMBALS,
    TAB_COUNT
} GuiTab;

// Parameter ID scheme. Global params sit below 100; every library owns a block
// starting at its param_base (Halpern 0, OKW 1000, PV 2000):
//   base + 1..4                    tab masters
//   base + 100 + ch*10 + offset    mixer channels
//   selector ids                   listed in each library descriptor
#define PARAM_MASTER_GAIN       0
#define PARAM_MIDI_MAP_MODE     5
#define PARAM_TAB_MASTER_OFFSET 1
#define PARAM_CHANNEL_OFFSET    100
#define PARAM_CH_GAIN           0
#define PARAM_CH_PAN            1
#define PARAM_CH_MUTE           2
#define PARAM_CH_SOLO           3
#define PARAM_CH_PHASE          4
#define PARAM_CH_STEREO         5
#define PARAM_CH_COUNT          6

// Output ports: port 0 = master (full mix), ports 1..N = the active library's
// mixer channels. The count is fixed so switching library never needs a
// port-count rescan; unused ports stay silent.
#define NUM_OUTPUT_PORTS (1 + MAX_LIB_CHANNELS)

// GM drum note -> GGD note remapping.
// Returns the GGD note for a GM note, or the note unchanged if no mapping exists.
typedef enum {
    MIDIMAP_GGD = 0,     // the pack's own layout, notes 24-83
    MIDIMAP_GM,          // General MIDI percussion
    MIDIMAP_GM_EXT,      // GGD's GM-style template used by their MIDI packs
    MIDIMAP_MODE_COUNT
} MidiMapMode;

static const char *MIDI_MAP_MODE_NAMES[MIDIMAP_MODE_COUNT] = {
    "GGD", "General MIDI", "GM Extended"
};

// Standard General MIDI percussion -> this pack's articulations.
static const int8_t MIDI_REMAP_GM[128] = {
    [35] = 24,  // Bass Drum 2 -> Kick
    [36] = 24,  // Bass Drum 1 -> Kick
    [37] = 30,  // Side Stick -> Stick Click
    [38] = 26,  // Snare -> Snare Hit
    [39] = 26,  // Hand Clap -> Snare Hit
    [40] = 26,  // Electric Snare -> Snare Hit
    [41] = 39,  // Low Floor Tom -> Floor Tom
    [42] = 47,  // Closed Hi-Hat -> Tip Closed
    [43] = 39,  // High Floor Tom -> Floor Tom
    [44] = 43,  // Pedal Hi-Hat -> Pedal Chik
    [45] = 37,  // Low Tom -> Mid Tom 2
    [46] = 55,  // Open Hi-Hat -> Tip Open 3
    [47] = 35,  // Low-Mid Tom -> Mid Tom 1
    [48] = 33,  // Hi-Mid Tom -> Hi Tom
    [49] = 62,  // Crash 1 -> Left Crash
    [50] = 33,  // High Tom -> Hi Tom
    [51] = 72,  // Ride 1 -> Ride Tip
    [52] = 76,  // Chinese Cymbal -> China
    [53] = 74,  // Ride Bell -> Ride Bell Tip
    [54] = 81,  // Tambourine -> Stack Tight
    [55] = 83,  // Splash -> Splash Hit
    [56] = 25,  // Cowbell -> Stick Click Kick
    [57] = 67,  // Crash 2 -> Right Crash
    [58] = 77,  // Vibraslap -> China Choke
    [59] = 73,  // Ride 2 -> Ride Crash
};

// GGD's own GM-style template, as shipped with their MIDI packs. GM-aligned at
// the anchors (36 kick, 38 snare, 42/44/46 hats, 49/51/52/53/55/57 cymbals) but
// with extra articulations layered on: a ghost snare, a second open hat, a
// third crash and a second splash.
static const int8_t MIDI_REMAP_GM_EXT[128] = {
    [35] = 24,  // (unlabelled, GM bass drum 2) -> Kick
    [36] = 24,  // Kick            -> Kick Main Hit
    [37] = 26,  // Snare (Ghost)   -> Snare Hit   (velocity picks the soft layer)
    [38] = 26,  // Snare           -> Snare Hit
    [39] = 30,  // Cross Stick     -> Stick Click
    [41] = 39,  // Floor Tom 2     -> Floor Tom
    [42] = 45,  // Tip Tight       -> Tip Tight
    [43] = 37,  // Floor Tom 1     -> Mid Tom 2
    [44] = 43,  // Pedal Hat       -> Pedal Chik
    [46] = 51,  // Open Hat 1      -> Tip Open 1
    [48] = 35,  // R Tom 2         -> Mid Tom 1
    [49] = 62,  // Crash Left      -> Left Crash
    [50] = 33,  // R Tom 1         -> Hi Tom
    [51] = 72,  // Ride Bow        -> Ride Tip
    [52] = 76,  // China           -> China Main Hit
    [53] = 74,  // Ride Bell       -> Ride Bell Tip
    [54] = 47,  // Tip Closed      -> Tip Closed
    [55] = 83,  // Splash 1        -> Splash Hit
    [56] = 83,  // Splash 2        -> Splash Hit  (pack has only one splash)
    [57] = 67,  // Crash Center    -> Right Crash
    [58] = 53,  // Open hat 2      -> Tip Open 2
    [60] = 67,  // Crash right     -> Right Crash (pack has only two crashes)
};

// Translate an incoming note for the selected map. Unmapped notes pass through
// unchanged, so GGD-native notes still work inside an otherwise-GM file.
static inline int midi_remap_note(int note, int mode) {
    if (note < 0 || note > 127) return note;
    const int8_t *m = NULL;
    if (mode == MIDIMAP_GM)          m = MIDI_REMAP_GM;
    else if (mode == MIDIMAP_GM_EXT) m = MIDI_REMAP_GM_EXT;
    if (m && m[note] != 0) return m[note];
    return note;
}

// Which tab does this drum type belong to?
static inline GuiTab drum_type_tab(DrumType drum) {
    switch (drum) {
    case DRUM_KICK:   return TAB_KICK;
    case DRUM_SNARE:  return TAB_SNARE;
    case DRUM_RACK1: case DRUM_RACK2:
    case DRUM_FLOOR1: case DRUM_FLOOR2:
        return TAB_TOMS;
    default: return TAB_CYMBALS;
    }
}

#endif
