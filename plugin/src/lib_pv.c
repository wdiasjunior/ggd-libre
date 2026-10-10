// GGD PV Matt Halpern Signature Pack: three kick mics, snare top/bottom, close
// mics on toms and most cymbals, overheads and three room pairs.
#include "library.h"

enum {
    MIC_KICK_IN = 0, MIC_KICK_OUT, MIC_KICK_PORT, MIC_SNARE_TOP, MIC_SNARE_BOTTOM,
    MIC_CLS, MIC_OH, MIC_ROOM, MIC_FAR_BLM, MIC_FAR_WIDE
};

enum {
    CH_KICK_IN = 0, CH_KICK_OUT, CH_KICK_PORT, CH_KICK_OH, CH_KICK_ROOM, CH_KICK_BLM, CH_KICK_WIDE,
    CH_SNARE_TOP, CH_SNARE_BOTTOM, CH_SNARE_OH, CH_SNARE_ROOM, CH_SNARE_BLM, CH_SNARE_WIDE,
    CH_TOM1, CH_TOM2, CH_TOM3, CH_TOM4, CH_TOMS_OH, CH_TOMS_ROOM, CH_TOMS_BLM, CH_TOMS_WIDE,
    CH_HIHAT, CH_RIDE, CH_CRASH, CH_SPLASH, CH_STACK,
    CH_CYMBALS_OH, CH_CYMBALS_ROOM, CH_CYMBALS_BLM, CH_CYMBALS_WIDE,
    CH_COUNT
};

enum { SEL_KICK = 0, SEL_SNARE, SEL_LCRASH, SEL_RCRASH, SEL_RIDE };

static int pv_route(DrumType drum, int mic) {
    if (drum == DRUM_ACCENT) return -1;
    if (mic >= MIC_OH) {
        int off = mic - MIC_OH;   // OH, room, far Blumlein, far wide
        switch (drum_type_tab(drum)) {
        case TAB_KICK:    return CH_KICK_OH + off;
        case TAB_SNARE:   return CH_SNARE_OH + off;
        case TAB_TOMS:    return CH_TOMS_OH + off;
        case TAB_CYMBALS: return CH_CYMBALS_OH + off;
        default:          return -1;
        }
    }
    switch (mic) {
    case MIC_KICK_IN:      return drum == DRUM_KICK ? CH_KICK_IN : -1;
    case MIC_KICK_OUT:     return drum == DRUM_KICK ? CH_KICK_OUT : -1;
    case MIC_KICK_PORT:    return drum == DRUM_KICK ? CH_KICK_PORT : -1;
    case MIC_SNARE_TOP:    return drum == DRUM_SNARE ? CH_SNARE_TOP : -1;
    case MIC_SNARE_BOTTOM: return drum == DRUM_SNARE ? CH_SNARE_BOTTOM : -1;
    default: break;
    }
    switch (drum) {   // MIC_CLS
    case DRUM_RACK1:  return CH_TOM1;
    case DRUM_RACK2:  return CH_TOM2;
    case DRUM_FLOOR1: return CH_TOM3;
    case DRUM_FLOOR2: return CH_TOM4;
    case DRUM_HIHAT:  return CH_HIHAT;
    case DRUM_RIDE:   return CH_RIDE;
    case DRUM_LCRASH:
    case DRUM_RCRASH: return CH_CRASH;   // only the 21" crash ride has a close mic
    case DRUM_SPLASH: return CH_SPLASH;
    case DRUM_STACK:  return CH_STACK;
    default:          return -1;
    }
}

#define CRASH_OPTIONS { "18\" Foundry Res", "18\" Trad Poly", "18\" Trad XThin", \
                        "19\" Foundry Res", "19\" Trad Poly", "21\" DD Crash Ride" }

const LibraryDef LIB_DEF_PV = {
    .slug = "pv_halpern",
    .name = "PV HALPERN",
    .param_prefix = "PV",
    .install_dirs = { "GGD PV Matt Halpern Signature Pack Samples", NULL },
    .param_base = 2000,

    .num_mics = 10,
    .mics = { "KckIn", "KckOut", "KckPrt", "SnrTop", "SnrBtm", "Cls", "OH",
              "RmCls", "RmFarBlm", "RmFarWde" },

    .num_channels = CH_COUNT,
    .channels = {
        [CH_KICK_IN]      = { "Kick In",           TAB_KICK,    "IN" },
        [CH_KICK_OUT]     = { "Kick Out",          TAB_KICK,    "OUT" },
        [CH_KICK_PORT]    = { "Kick Port",         TAB_KICK,    "PORT" },
        [CH_KICK_OH]      = { "Kick OH",           TAB_KICK,    "OH" },
        [CH_KICK_ROOM]    = { "Kick Room",         TAB_KICK,    "ROOM" },
        [CH_KICK_BLM]     = { "Kick Far Blumlein", TAB_KICK,    "FAR BLM" },
        [CH_KICK_WIDE]    = { "Kick Far Wide",     TAB_KICK,    "FAR WIDE" },
        [CH_SNARE_TOP]    = { "Snare Top",         TAB_SNARE,   "TOP" },
        [CH_SNARE_BOTTOM] = { "Snare Bottom",      TAB_SNARE,   "BOTTOM" },
        [CH_SNARE_OH]     = { "Snare OH",          TAB_SNARE,   "OH" },
        [CH_SNARE_ROOM]   = { "Snare Room",        TAB_SNARE,   "ROOM" },
        [CH_SNARE_BLM]    = { "Snare Far Blumlein",TAB_SNARE,   "FAR BLM" },
        [CH_SNARE_WIDE]   = { "Snare Far Wide",    TAB_SNARE,   "FAR WIDE" },
        [CH_TOM1]         = { "Tom 1",             TAB_TOMS,    "TOM 1" },
        [CH_TOM2]         = { "Tom 2",             TAB_TOMS,    "TOM 2" },
        [CH_TOM3]         = { "Tom 3",             TAB_TOMS,    "TOM 3" },
        [CH_TOM4]         = { "Tom 4",             TAB_TOMS,    "TOM 4" },
        [CH_TOMS_OH]      = { "Toms OH",           TAB_TOMS,    "OH" },
        [CH_TOMS_ROOM]    = { "Toms Room",         TAB_TOMS,    "ROOM" },
        [CH_TOMS_BLM]     = { "Toms Far Blumlein", TAB_TOMS,    "FAR BLM" },
        [CH_TOMS_WIDE]    = { "Toms Far Wide",     TAB_TOMS,    "FAR WIDE" },
        [CH_HIHAT]        = { "Hi-Hat",            TAB_CYMBALS, "HI-HAT" },
        [CH_RIDE]         = { "Ride",              TAB_CYMBALS, "RIDE" },
        [CH_CRASH]        = { "Crash Close",       TAB_CYMBALS, "CRASH" },
        [CH_SPLASH]       = { "Splash",            TAB_CYMBALS, "SPLASH" },
        [CH_STACK]        = { "Stack",             TAB_CYMBALS, "STACK" },
        [CH_CYMBALS_OH]   = { "Cymbals OH",        TAB_CYMBALS, "OH" },
        [CH_CYMBALS_ROOM] = { "Cymbals Room",      TAB_CYMBALS, "ROOM" },
        [CH_CYMBALS_BLM]  = { "Cymbals Far Blumlein", TAB_CYMBALS, "FAR BLM" },
        [CH_CYMBALS_WIDE] = { "Cymbals Far Wide",  TAB_CYMBALS, "FAR WIDE" },
    },
    .route = pv_route,

    .num_selectors = 5,
    .selectors = {
        [SEL_KICK]   = { "kick",         "Kick",         2900, 0, 2, { "Reference", "Masters Birch" } },
        [SEL_SNARE]  = { "snare_tuning", "Snare Tuning", 2901, 1, 3, { "Low D", "Mid E", "High F#" } },
        [SEL_LCRASH] = { "lcrash",       "L Crash",      2902, 0, 6, CRASH_OPTIONS },
        [SEL_RCRASH] = { "rcrash",       "R Crash",      2903, 3, 6, CRASH_OPTIONS },
        [SEL_RIDE]   = { "ride",         "Ride",         2904, 0, 2, { "21\" DD Crash Ride", "15\" Big Bell" } },
    },

    .num_items = { 1, 1, 4, 7 },
    .items = {
        [TAB_KICK]  = { { "Kick", 24, SEL_KICK, NULL } },
        [TAB_SNARE] = { { "Snare Tuning", 26, SEL_SNARE, NULL } },
        [TAB_TOMS]  = {
            { "Tom 1", 33, -1, "12x8 Reference" }, { "Tom 2", 35, -1, "13x9 Reference" },
            { "Tom 3", 37, -1, "16x16 Reference" }, { "Tom 4", 39, -1, "18x16 Reference" },
        },
        [TAB_CYMBALS] = {
            { "Hi-Hat",  47, -1,         "15\" Byz Trad Med" },
            { "L Crash", 62, SEL_LCRASH, NULL },
            { "R Crash", 67, SEL_RCRASH, NULL },
            { "Ride",    72, SEL_RIDE,   NULL },
            { "Stack",   81, -1,         "17/18\" DD Stack" },
            { "Splash",  83, -1,         "10\" Byz Trad" },
            { "China",   76, -1,         "20\" Byz Trad" },
        },
    },
};
