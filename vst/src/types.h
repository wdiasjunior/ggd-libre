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

// Mic positions in sample directories
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

// Mixer channels — these are the faders shown in the UI
typedef enum {
    // Close mic channels (per drum piece)
    CH_KICK_CLOSE = 0,
    CH_SNARE_TOP1,
    CH_SNARE_TOP2,
    CH_SNARE_BOTTOM,
    CH_RACK1,
    CH_RACK2,
    CH_FLOOR1,
    CH_FLOOR2,
    CH_HIHAT,
    CH_RIDE,
    CH_STACK,
    CH_SPLASH,
    CH_CHINA,
    // Room mic channels (shared across all drums)
    CH_OH,
    CH_NEAR_ROOM,
    CH_FAR_ROOM,
    MIXER_CHANNEL_COUNT  // = 16
} MixerChannel;

static const char *MIXER_CHANNEL_NAMES[MIXER_CHANNEL_COUNT] = {
    "Kick Close",
    "Snare Top 1", "Snare Top 2", "Snare Bottom",
    "Rack 1", "Rack 2", "Floor 1", "Floor 2",
    "Hi-Hat", "Ride", "Stack", "Splash", "China",
    "OH", "Near Room", "Far Room"
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
#define PARAM_VAR_LCRASH_SIZE   308
#define PARAM_VAR_RCRASH_SIZE   309
#define PARAM_VAR_COUNT         10

#define TOTAL_PARAMS (1 + MIXER_CHANNEL_COUNT * PARAM_CH_COUNT + PARAM_VAR_COUNT)

static inline uint32_t param_channel_id(MixerChannel ch, int offset) {
    return PARAM_CHANNEL_BASE + ch * 10 + offset;
}

static inline bool param_is_channel(uint32_t id, MixerChannel *ch, int *offset) {
    if (id < PARAM_CHANNEL_BASE || id >= PARAM_CHANNEL_BASE + MIXER_CHANNEL_COUNT * 10)
        return false;
    uint32_t rel = id - PARAM_CHANNEL_BASE;
    *ch = (MixerChannel)(rel / 10);
    *offset = rel % 10;
    return *offset < PARAM_CH_COUNT;
}

// Route a drum type + mic position to the correct mixer channel.
// Returns -1 if this drum type doesn't use this mic position.
static inline int get_mixer_channel(DrumType drum, MicPosition mic) {
    // Room mics are shared across all drums
    switch (mic) {
    case MIC_OH:        return CH_OH;
    case MIC_NEAR_ROOM: return CH_NEAR_ROOM;
    case MIC_FAR_ROOM:  return CH_FAR_ROOM;
    default: break;
    }
    // Close mics route per drum type
    switch (drum) {
    case DRUM_KICK:   return (mic == MIC_CLOSE) ? CH_KICK_CLOSE : -1;
    case DRUM_SNARE:
        switch (mic) {
        case MIC_TOP1:   return CH_SNARE_TOP1;
        case MIC_TOP2:   return CH_SNARE_TOP2;
        case MIC_BOTTOM: return CH_SNARE_BOTTOM;
        default:         return -1;
        }
    case DRUM_RACK1:  return (mic == MIC_CLOSE) ? CH_RACK1 : -1;
    case DRUM_RACK2:  return (mic == MIC_CLOSE) ? CH_RACK2 : -1;
    case DRUM_FLOOR1: return (mic == MIC_CLOSE) ? CH_FLOOR1 : -1;
    case DRUM_FLOOR2: return (mic == MIC_CLOSE) ? CH_FLOOR2 : -1;
    case DRUM_HIHAT:  return (mic == MIC_CLOSE) ? CH_HIHAT : -1;
    case DRUM_RIDE:   return (mic == MIC_CLOSE) ? CH_RIDE : -1;
    case DRUM_CHINA:  return (mic == MIC_CLOSE) ? CH_CHINA : -1;
    case DRUM_STACK:  return (mic == MIC_CLOSE) ? CH_STACK : -1;
    case DRUM_SPLASH: return (mic == MIC_CLOSE) ? CH_SPLASH : -1;
    case DRUM_LCRASH: return -1; // crashes have no close mic
    case DRUM_RCRASH: return -1;
    case DRUM_ACCENT: return -1;
    default:          return -1;
    }
}

#endif
