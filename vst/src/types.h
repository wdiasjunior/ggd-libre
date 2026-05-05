#ifndef GGD_TYPES_H
#define GGD_TYPES_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_MIC_POSITIONS    7
#define MAX_VELOCITY_LAYERS  10
#define MAX_ROUND_ROBINS     7
#define MAX_VOICES           64
#define MAX_MIDI_NOTES       128
#define MAX_ARTICULATIONS    128
#define MAX_VARIANTS         8
#define CHOKE_FADE_MS        5.0f

typedef enum {
    MIC_CLOSE = 0,
    MIC_OH,
    MIC_NEAR_ROOM,
    MIC_FAR_ROOM,
    MIC_BOTTOM,
    MIC_TOP1,
    MIC_TOP2,
    MIC_COUNT
} MicPosition;

static const char *MIC_DIR_NAMES[MIC_COUNT] = {
    "CloseMic",
    "OHMic",
    "NearRoomMic",
    "FarRoomMic",
    "BottomMic",
    "TopMic1",
    "TopMic2"
};

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
    DRUM_CHANNEL_COUNT
} DrumChannel;

static const char *DRUM_CHANNEL_NAMES[DRUM_CHANNEL_COUNT] = {
    "Kick", "Snare",
    "Rack 1", "Rack 2", "Floor 1", "Floor 2",
    "Hi-Hat", "L Crash", "R Crash", "Ride",
    "China", "Stack", "Splash", "Accent"
};

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

// Parameter ID scheme
#define PARAM_MASTER_GAIN       0

// Per-channel: base + channel*10 + offset
#define PARAM_CHANNEL_BASE      100
#define PARAM_CH_GAIN           0
#define PARAM_CH_PAN            1
#define PARAM_CH_MUTE           2
#define PARAM_CH_SOLO           3
#define PARAM_CH_PHASE          4
#define PARAM_CH_STEREO         5
#define PARAM_CH_COUNT          6

// Variant selectors
#define PARAM_VAR_BASE          300
#define PARAM_VAR_KICK_SIZE     300
#define PARAM_VAR_SNARE_TYPE    301
#define PARAM_VAR_TOM1_HEAD     302
#define PARAM_VAR_TOM2_HEAD     303
#define PARAM_VAR_TOM3_HEAD     304
#define PARAM_VAR_TOM4_HEAD     305
#define PARAM_VAR_CHINA_SIZE    306
#define PARAM_VAR_STACK_TYPE    307
#define PARAM_VAR_COUNT         8

#define TOTAL_PARAMS (1 + DRUM_CHANNEL_COUNT * PARAM_CH_COUNT + PARAM_VAR_COUNT)

static inline uint32_t param_channel_id(DrumChannel ch, int offset) {
    return PARAM_CHANNEL_BASE + ch * 10 + offset;
}

static inline bool param_is_channel(uint32_t id, DrumChannel *ch, int *offset) {
    if (id < PARAM_CHANNEL_BASE || id >= PARAM_CHANNEL_BASE + DRUM_CHANNEL_COUNT * 10)
        return false;
    uint32_t rel = id - PARAM_CHANNEL_BASE;
    *ch = (DrumChannel)(rel / 10);
    *offset = rel % 10;
    return *offset < PARAM_CH_COUNT;
}

#endif
