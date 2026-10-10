#include "midi_map.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// All libraries share the Halpern note layout, so drum type and choke
// behaviour follow from the note number alone.
static DrumType classify_drum_type(int midi_note) {
    if (midi_note == 24 || midi_note == 25) return DRUM_KICK;
    if (midi_note >= 26 && midi_note <= 32) return DRUM_SNARE;
    if (midi_note == 33 || midi_note == 34) return DRUM_RACK1;
    if (midi_note == 35 || midi_note == 36) return DRUM_RACK2;
    if (midi_note == 37 || midi_note == 38) return DRUM_FLOOR1;
    if (midi_note == 39 || midi_note == 40) return DRUM_FLOOR2;
    if (midi_note >= 43 && midi_note <= 58) return DRUM_HIHAT;
    if (midi_note >= 62 && midi_note <= 65) return DRUM_LCRASH;
    if (midi_note >= 67 && midi_note <= 70) return DRUM_RCRASH;
    if (midi_note >= 72 && midi_note <= 75) return DRUM_RIDE;
    if (midi_note == 76 || midi_note == 77) return DRUM_CHINA;
    if (midi_note == 81 || midi_note == 82) return DRUM_STACK;
    if (midi_note == 83) return DRUM_SPLASH;
    return DRUM_ACCENT;
}

static ChokeGroup get_choke_group(int midi_note) {
    if (midi_note >= 43 && midi_note <= 58) return CHOKE_HIHAT;
    if (midi_note >= 62 && midi_note <= 65) return CHOKE_LCRASH;
    if (midi_note >= 67 && midi_note <= 70) return CHOKE_RCRASH;
    if (midi_note == 76 || midi_note == 77) return CHOKE_CHINA;
    return CHOKE_NONE;
}

static bool is_choke_trigger(int midi_note) {
    if (midi_note >= 43 && midi_note <= 48) return true;
    if (midi_note == 64) return true;
    if (midi_note == 69) return true;
    if (midi_note == 77) return true;
    return false;
}

static void parse_art_list(ArtList *out, const SampleBank *bank, const cJSON *arr,
                           const char *context) {
    out->count = 0;
    const cJSON *item;
    cJSON_ArrayForEach(item, arr) {
        if (!cJSON_IsString(item) || out->count >= MAX_NOTE_ARTS) continue;
        int idx = sample_bank_find(bank, item->valuestring);
        if (idx < 0) {
            fprintf(stderr, "ggd-libre: %s: articulation '%s' not in bank\n",
                    context, item->valuestring);
            continue;
        }
        out->arts[out->count++] = idx;
    }
}

static int note_number(const char *key) {
    char *end;
    long n = strtol(key, &end, 10);
    if (end == key || *end || n < 0 || n >= MAX_MIDI_NOTES) return -1;
    return (int)n;
}

bool midi_map_load(MidiMap *map, const SampleBank *bank, const cJSON *notes,
                   const cJSON *selectors, const char *const *selector_keys,
                   int num_selectors) {
    memset(map, 0, sizeof(*map));
    for (int n = 0; n < MAX_MIDI_NOTES; n++) {
        MidiNoteSlot *slot = &map->slots[n];
        slot->selector = -1;
        slot->drum_type = classify_drum_type(n);
        slot->choke_group = get_choke_group(n);
        slot->is_choke_trigger = is_choke_trigger(n);
    }

    const cJSON *entry;
    cJSON_ArrayForEach(entry, notes) {
        int n = note_number(entry->string);
        if (n < 0) continue;
        MidiNoteSlot *slot = &map->slots[n];
        const cJSON *name = cJSON_GetObjectItem(entry, "name");
        if (cJSON_IsString(name))
            strncpy(slot->name, name->valuestring, sizeof(slot->name) - 1);
        parse_art_list(&slot->play, bank, cJSON_GetObjectItem(entry, "play"), entry->string);
    }

    for (int s = 0; s < num_selectors; s++) {
        const cJSON *opts = cJSON_GetObjectItem(selectors, selector_keys[s]);
        if (!opts) {
            fprintf(stderr, "ggd-libre: index has no selector '%s'\n", selector_keys[s]);
            continue;
        }
        int o = 0;
        const cJSON *opt;
        cJSON_ArrayForEach(opt, opts) {
            if (o >= MAX_MAP_OPTIONS) break;
            const cJSON *note_entry;
            cJSON_ArrayForEach(note_entry, opt) {
                int n = note_number(note_entry->string);
                if (n < 0) continue;
                MidiNoteSlot *slot = &map->slots[n];
                if (slot->selector >= 0 && slot->selector != s)
                    fprintf(stderr, "ggd-libre: note %d claimed by selectors %d and %d\n",
                            n, slot->selector, s);
                slot->selector = s;
                parse_art_list(&slot->options[o], bank, note_entry, selector_keys[s]);
            }
            o++;
        }
    }

    int mapped = 0;
    for (int n = 0; n < MAX_MIDI_NOTES; n++) {
        MidiNoteSlot *slot = &map->slots[n];
        bool any = slot->play.count > 0;
        for (int o = 0; o < MAX_MAP_OPTIONS && !any; o++) any = slot->options[o].count > 0;
        if (any) mapped++;
        else slot->name[0] = '\0';
    }
    fprintf(stderr, "ggd-libre: %d MIDI notes mapped\n", mapped);
    return mapped > 0;
}
