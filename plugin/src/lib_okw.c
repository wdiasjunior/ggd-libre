// GGD One Kit Wonder - Metal: one kit, close / overhead / room (M-S) mics.
#include "library.h"

enum { MIC_CLS = 0, MIC_OH, MIC_RM };

enum {
    CH_KICK_CLOSE = 0, CH_KICK_OH, CH_KICK_ROOM,
    CH_SNARE_CLOSE, CH_SNARE_OH, CH_SNARE_ROOM,
    CH_TOM1, CH_TOM2, CH_TOM3, CH_TOM4, CH_TOMS_OH, CH_TOMS_ROOM,
    CH_HIHAT, CH_RIDE, CH_SPLASH, CH_CYMBALS_OH, CH_CYMBALS_ROOM,
    CH_COUNT
};

enum { SEL_LCRASH = 0, SEL_RCRASH, SEL_SPLASH };

static int okw_route(DrumType drum, int mic) {
    if (drum == DRUM_ACCENT) return -1;
    if (mic == MIC_OH || mic == MIC_RM) {
        int room = (mic == MIC_RM);
        switch (drum_type_tab(drum)) {
        case TAB_KICK:    return CH_KICK_OH + room;
        case TAB_SNARE:   return CH_SNARE_OH + room;
        case TAB_TOMS:    return CH_TOMS_OH + room;
        case TAB_CYMBALS: return CH_CYMBALS_OH + room;
        default:          return -1;
        }
    }
    switch (drum) {
    case DRUM_KICK:   return CH_KICK_CLOSE;
    case DRUM_SNARE:  return CH_SNARE_CLOSE;
    case DRUM_RACK1:  return CH_TOM1;
    case DRUM_RACK2:  return CH_TOM2;
    case DRUM_FLOOR1: return CH_TOM3;
    case DRUM_FLOOR2: return CH_TOM4;
    case DRUM_HIHAT:  return CH_HIHAT;
    case DRUM_RIDE:   return CH_RIDE;
    case DRUM_SPLASH:
    case DRUM_STACK:  return CH_SPLASH;   // the stack notes play the 10" splash
    default:          return -1;          // crashes and china have no close mic
    }
}

const LibraryDef LIB_DEF_OKW = {
    .slug = "okw_metal",
    .name = "ONE KIT WONDER",
    .param_prefix = "OKW",
    .install_dirs = { "GGD One Kit Wonder Metal Samples", NULL },
    .param_base = 1000,

    .num_mics = 3,
    .mics = { "Cls", "OH", "Rm" },

    .num_channels = CH_COUNT,
    .channels = {
        [CH_KICK_CLOSE]   = { "Kick Close",   TAB_KICK,    "CLOSE" },
        [CH_KICK_OH]      = { "Kick OH",      TAB_KICK,    "OH" },
        [CH_KICK_ROOM]    = { "Kick Room",    TAB_KICK,    "ROOM" },
        [CH_SNARE_CLOSE]  = { "Snare Close",  TAB_SNARE,   "CLOSE" },
        [CH_SNARE_OH]     = { "Snare OH",     TAB_SNARE,   "OH" },
        [CH_SNARE_ROOM]   = { "Snare Room",   TAB_SNARE,   "ROOM" },
        [CH_TOM1]         = { "Tom 1",        TAB_TOMS,    "TOM 1" },
        [CH_TOM2]         = { "Tom 2",        TAB_TOMS,    "TOM 2" },
        [CH_TOM3]         = { "Tom 3",        TAB_TOMS,    "TOM 3" },
        [CH_TOM4]         = { "Tom 4",        TAB_TOMS,    "TOM 4" },
        [CH_TOMS_OH]      = { "Toms OH",      TAB_TOMS,    "OH" },
        [CH_TOMS_ROOM]    = { "Toms Room",    TAB_TOMS,    "ROOM" },
        [CH_HIHAT]        = { "Hi-Hat",       TAB_CYMBALS, "HI-HAT" },
        [CH_RIDE]         = { "Ride",         TAB_CYMBALS, "RIDE" },
        [CH_SPLASH]       = { "Splash",       TAB_CYMBALS, "SPLASH" },
        [CH_CYMBALS_OH]   = { "Cymbals OH",   TAB_CYMBALS, "OH" },
        [CH_CYMBALS_ROOM] = { "Cymbals Room", TAB_CYMBALS, "ROOM" },
    },
    .route = okw_route,

    .num_selectors = 3,
    .selectors = {
        [SEL_LCRASH] = { "lcrash", "L Crash", 1900, 1, 3, { "17\" F602 ME", "18\" F602 ME", "19\" F602 ME" } },
        [SEL_RCRASH] = { "rcrash", "R Crash", 1901, 2, 3, { "17\" F602 ME", "18\" F602 ME", "19\" F602 ME" } },
        [SEL_SPLASH] = { "splash", "Splash",  1902, 0, 2, { "8\" 2002", "10\" 2002" } },
    },

    .num_items = { 1, 1, 4, 7 },
    .items = {
        [TAB_KICK]  = { { "Kick", 24, -1, "22x16 Starclassic" } },
        [TAB_SNARE] = { { "Snare", 26, -1, "14x8 Pearl VP Sig" } },
        [TAB_TOMS]  = {
            { "Tom 1", 33, -1, "10x9 Starclassic" }, { "Tom 2", 35, -1, "12x10 Starclassic" },
            { "Tom 3", 37, -1, "14x14 Starclassic" }, { "Tom 4", 39, -1, "16x16 Starclassic" },
        },
        [TAB_CYMBALS] = {
            { "Hi-Hat",  47, -1,         "14\" TC Metal" },
            { "L Crash", 62, SEL_LCRASH, NULL },
            { "R Crash", 67, SEL_RCRASH, NULL },
            { "Ride",    72, -1,         "22\" TC Metal" },
            { "Stack",   81, -1,         "10\" 2002 Splash" },
            { "Splash",  83, SEL_SPLASH, NULL },
            { "China",   76, -1,         "18\" TC Thin" },
        },
    },
};
