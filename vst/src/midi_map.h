#ifndef GGD_MIDI_MAP_H
#define GGD_MIDI_MAP_H

#include "types.h"
#include "sample_bank.h"

typedef struct {
    int          articulation_index;  // into SampleBank
    DrumChannel  drum_channel;
    ChokeGroup   choke_group;
    bool         is_choke_trigger;    // true = this note kills voices in the group
} NoteVariant;

typedef struct {
    char        name[64];
    NoteVariant variants[MAX_VARIANTS];
    int         num_variants;
    int         active_variant;
} MidiNoteSlot;

typedef struct {
    MidiNoteSlot slots[MAX_MIDI_NOTES];
} MidiMap;

bool midi_map_load(MidiMap *map, const SampleBank *bank, const char *json_path);
void midi_map_update_variants(MidiMap *map, const SampleBank *bank,
                              int kick_size, int snare_type,
                              int tom_head[4], int china_size, int stack_type);

#endif
