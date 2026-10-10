// GGD Matt Halpern Signature Pack.
// Channel order and param ids are the plugin's original layout, so existing
// sessions and automation keep working.
#include "library.h"

enum {
    MIC_CLOSE = 0, MIC_OH, MIC_NEAR_ROOM, MIC_FAR_ROOM, MIC_BOTTOM, MIC_TOP1, MIC_TOP2
};

enum {
    CH_KICK_CLOSE = 0, CH_KICK_OH, CH_KICK_NEAR, CH_KICK_FAR,
    CH_SNARE_TOP1, CH_SNARE_TOP2, CH_SNARE_BOTTOM, CH_SNARE_OH, CH_SNARE_NEAR, CH_SNARE_FAR,
    CH_RACK1, CH_RACK2, CH_FLOOR1, CH_FLOOR2, CH_TOMS_OH, CH_TOMS_NEAR, CH_TOMS_FAR,
    CH_HIHAT, CH_LCRASH, CH_RCRASH, CH_RIDE, CH_STACK, CH_SPLASH, CH_CHINA,
    CH_CYMBALS_OH, CH_CYMBALS_NEAR, CH_CYMBALS_FAR,
    CH_COUNT
};

enum {
    SEL_KICK = 0, SEL_SNARE, SEL_TOM1, SEL_TOM2, SEL_TOM3, SEL_TOM4,
    SEL_CHINA, SEL_STACK, SEL_LCRASH, SEL_RCRASH
};

static int halpern_route(DrumType drum, int mic) {
    // Crashes route ALL their mics (OH/Near/Far) to a single crash channel
    if (drum == DRUM_LCRASH) return (mic == MIC_CLOSE) ? -1 : CH_LCRASH;
    if (drum == DRUM_RCRASH) return (mic == MIC_CLOSE) ? -1 : CH_RCRASH;

    // Room mics route to per-tab channels
    if (mic == MIC_OH || mic == MIC_NEAR_ROOM || mic == MIC_FAR_ROOM) {
        int base;
        switch (drum_type_tab(drum)) {
        case TAB_KICK:    base = CH_KICK_OH; break;
        case TAB_SNARE:   base = CH_SNARE_OH; break;
        case TAB_TOMS:    base = CH_TOMS_OH; break;
        case TAB_CYMBALS: base = CH_CYMBALS_OH; break;
        default: return -1;
        }
        return base + (mic == MIC_OH ? 0 : mic == MIC_NEAR_ROOM ? 1 : 2);
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
    default:          return -1;
    }
}

const LibraryDef LIB_DEF_HALPERN = {
    .slug = "halpern",
    .name = "MATT HALPERN",
    .param_prefix = "",
    .install_dirs = { "GGD Matt Halpern Signature Pack", NULL },
    .param_base = 0,

    .num_mics = 7,
    .mics = { "CloseMic", "OHMic", "NearRoomMic", "FarRoomMic", "BottomMic", "TopMic1", "TopMic2" },

    .num_channels = CH_COUNT,
    .channels = {
        [CH_KICK_CLOSE]   = { "Kick Close",   TAB_KICK,    "CLOSE" },
        [CH_KICK_OH]      = { "Kick OH",      TAB_KICK,    "OH" },
        [CH_KICK_NEAR]    = { "Kick Near",    TAB_KICK,    "NEAR ROOM" },
        [CH_KICK_FAR]     = { "Kick Far",     TAB_KICK,    "FAR ROOM" },
        [CH_SNARE_TOP1]   = { "Snare Top 1",  TAB_SNARE,   "TOP MIC 1" },
        [CH_SNARE_TOP2]   = { "Snare Top 2",  TAB_SNARE,   "TOP MIC 2" },
        [CH_SNARE_BOTTOM] = { "Snare Bottom", TAB_SNARE,   "BOTTOM" },
        [CH_SNARE_OH]     = { "Snare OH",     TAB_SNARE,   "OH" },
        [CH_SNARE_NEAR]   = { "Snare Near",   TAB_SNARE,   "NEAR ROOM" },
        [CH_SNARE_FAR]    = { "Snare Far",    TAB_SNARE,   "FAR ROOM" },
        [CH_RACK1]        = { "Rack 1",       TAB_TOMS,    "RACK 1" },
        [CH_RACK2]        = { "Rack 2",       TAB_TOMS,    "RACK 2" },
        [CH_FLOOR1]       = { "Floor 1",      TAB_TOMS,    "FLOOR 1" },
        [CH_FLOOR2]       = { "Floor 2",      TAB_TOMS,    "FLOOR 2" },
        [CH_TOMS_OH]      = { "Toms OH",      TAB_TOMS,    "OH" },
        [CH_TOMS_NEAR]    = { "Toms Near",    TAB_TOMS,    "NEAR ROOM" },
        [CH_TOMS_FAR]     = { "Toms Far",     TAB_TOMS,    "FAR ROOM" },
        [CH_HIHAT]        = { "Hi-Hat",       TAB_CYMBALS, "HI-HAT" },
        [CH_LCRASH]       = { "L Crash",      TAB_CYMBALS, "L CRASH" },
        [CH_RCRASH]       = { "R Crash",      TAB_CYMBALS, "R CRASH" },
        [CH_RIDE]         = { "Ride",         TAB_CYMBALS, "RIDE" },
        [CH_STACK]        = { "Stack",        TAB_CYMBALS, "STACK" },
        [CH_SPLASH]       = { "Splash",       TAB_CYMBALS, "SPLASH" },
        [CH_CHINA]        = { "China",        TAB_CYMBALS, "CHINA" },
        [CH_CYMBALS_OH]   = { "Cymbals OH",   TAB_CYMBALS, "OH" },
        [CH_CYMBALS_NEAR] = { "Cymbals Near", TAB_CYMBALS, "NEAR ROOM" },
        [CH_CYMBALS_FAR]  = { "Cymbals Far",  TAB_CYMBALS, "FAR ROOM" },
    },
    .route = halpern_route,

    // Ids 306-309 are the original ones. Kick/snare/toms used 300-305, which
    // collided with the Ride channel's ids, so they moved to 390-395.
    .num_selectors = 10,
    .selectors = {
        [SEL_KICK]   = { "kick_size",  "Kick Size",  390, 0, 2, { "22x16", "22x20" } },
        [SEL_SNARE]  = { "snare_type", "Snare Type", 391, 0, 5, { "High", "Med", "Low", "13\"", "BFSD" } },
        [SEL_TOM1]   = { "tom1_head",  "Tom 1 Head", 392, 0, 2, { "Clear", "Coated" } },
        [SEL_TOM2]   = { "tom2_head",  "Tom 2 Head", 393, 0, 2, { "Clear", "Coated" } },
        [SEL_TOM3]   = { "tom3_head",  "Tom 3 Head", 394, 0, 2, { "Clear", "Coated" } },
        [SEL_TOM4]   = { "tom4_head",  "Tom 4 Head", 395, 0, 2, { "Clear", "Coated" } },
        [SEL_CHINA]  = { "china",      "China Size", 306, 0, 2, { "China", "18\" China" } },
        [SEL_STACK]  = { "stack",      "Stack Type", 307, 0, 2, { "Stack", "Mini Stack" } },
        [SEL_LCRASH] = { "lcrash",     "L Crash",    308, 0, 2, { "17\" Byz Thin", "18\" Med Byz" } },
        [SEL_RCRASH] = { "rcrash",     "R Crash",    309, 0, 2, { "20\" Byz Thin", "19\" Med Byz" } },
    },

    .num_items = { 1, 1, 4, 7 },
    .items = {
        [TAB_KICK]  = { { "Kick Size", 24, SEL_KICK, NULL } },
        [TAB_SNARE] = { { "Snare Type", 26, SEL_SNARE, NULL } },
        [TAB_TOMS]  = {
            { "Tom 1", 33, SEL_TOM1, NULL }, { "Tom 2", 35, SEL_TOM2, NULL },
            { "Tom 3", 37, SEL_TOM3, NULL }, { "Tom 4", 39, SEL_TOM4, NULL },
        },
        [TAB_CYMBALS] = {
            { "Hi-Hat",  47, -1,         "14\" Paiste 2002" },
            { "L Crash", 62, SEL_LCRASH, NULL },
            { "R Crash", 67, SEL_RCRASH, NULL },
            { "Ride",    72, -1,         "22\" Paiste Ride" },
            { "Stack",   81, SEL_STACK,  NULL },
            { "Splash",  83, -1,         "Paiste 2002 Splash" },
            { "China",   76, SEL_CHINA,  NULL },
        },
    },
};
