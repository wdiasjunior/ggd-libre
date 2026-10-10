#ifndef GGD_MIDI_MAP_H
#define GGD_MIDI_MAP_H

#include "types.h"
#include "sample_bank.h"

#define MAX_NOTE_ARTS 8   // articulations one note fires at once (Halpern kick: 2)
#define MAX_MAP_OPTIONS 8 // must be >= MAX_SEL_OPTIONS

typedef struct {
    int arts[MAX_NOTE_ARTS];   // indices into SampleBank
    int count;
} ArtList;

// Everything a note does is precomputed at load: the default articulations and,
// when a selector governs the note, one list per selector option. The audio
// thread only reads this, so changing a selector never mutates shared state.
typedef struct {
    char       name[64];
    DrumType   drum_type;
    ChokeGroup choke_group;
    bool       is_choke_trigger;
    ArtList    play;                       // used when no selector applies
    int        selector;                   // index into the library's selectors, or -1
    ArtList    options[MAX_MAP_OPTIONS];   // per option; count 0 falls back to play
} MidiNoteSlot;

typedef struct {
    MidiNoteSlot slots[MAX_MIDI_NOTES];
} MidiMap;

// selector_keys/num_selectors: the library descriptor's selectors, in order.
bool midi_map_load(MidiMap *map, const SampleBank *bank, const cJSON *notes,
                   const cJSON *selectors, const char *const *selector_keys,
                   int num_selectors);

// The articulations a note fires for the given selector values.
static inline const ArtList *midi_map_resolve(const MidiMap *map, int note,
                                              const int *selector_values) {
    const MidiNoteSlot *slot = &map->slots[note];
    if (slot->selector >= 0) {
        int o = selector_values[slot->selector];
        if (o >= 0 && o < MAX_MAP_OPTIONS && slot->options[o].count > 0)
            return &slot->options[o];
    }
    return &slot->play;
}

#endif
