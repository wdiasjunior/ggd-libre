#include "midi_map.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

bool midi_map_load(MidiMap *map, const SampleBank *bank, const char *json_path) {
    memset(map, 0, sizeof(*map));

    FILE *f = fopen(json_path, "rb");
    if (!f) {
        fprintf(stderr, "ggd-libre: cannot open %s\n", json_path);
        return false;
    }

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *json_str = malloc(fsize + 1);
    if (!json_str) { fclose(f); return false; }
    fread(json_str, 1, fsize, f);
    json_str[fsize] = '\0';
    fclose(f);

    cJSON *root = cJSON_Parse(json_str);
    free(json_str);
    if (!root) {
        fprintf(stderr, "ggd-libre: JSON parse error\n");
        return false;
    }

    cJSON *entry;
    cJSON_ArrayForEach(entry, root) {
        int midi_note = atoi(entry->string);
        if (midi_note < 0 || midi_note >= MAX_MIDI_NOTES) continue;

        MidiNoteSlot *slot = &map->slots[midi_note];

        cJSON *name_item = cJSON_GetObjectItem(entry, "name");
        if (name_item && name_item->valuestring)
            strncpy(slot->name, name_item->valuestring, sizeof(slot->name) - 1);

        cJSON *samples_arr = cJSON_GetObjectItem(entry, "samples");
        if (!samples_arr) continue;

        cJSON *sample;
        cJSON_ArrayForEach(sample, samples_arr) {
            if (slot->num_variants >= MAX_VARIANTS) break;

            cJSON *prefix_item = cJSON_GetObjectItem(sample, "sample_prefix");
            if (!prefix_item || !prefix_item->valuestring) continue;

            int art_idx = sample_bank_find(bank, prefix_item->valuestring);
            if (art_idx < 0) {
                fprintf(stderr, "ggd-libre: sample prefix '%s' not found in bank\n",
                        prefix_item->valuestring);
                continue;
            }

            NoteVariant *var = &slot->variants[slot->num_variants++];
            var->articulation_index = art_idx;
            var->drum_type = classify_drum_type(midi_note);
            var->choke_group = get_choke_group(midi_note);
            var->is_choke_trigger = is_choke_trigger(midi_note);
        }
    }

    cJSON_Delete(root);

    int mapped = 0;
    for (int i = 0; i < MAX_MIDI_NOTES; i++) {
        if (map->slots[i].num_variants > 0) mapped++;
    }
    fprintf(stderr, "ggd-libre: %d MIDI notes mapped\n", mapped);
    return mapped > 0;
}

// Snare variant prefix mapping:
// [snare_type][articulation: 0=hit, 1=flam, 2=ruff, 3=off, 4=click/stick]
static const char *SNARE_PREFIXES[5][5] = {
    // High
    { "SnareHigh", "SnareHigh_Flam", "SnareHigh_Ruff", "SnareHigh_Off", "SnareHigh_click" },
    // Med
    { "SnareMed",  "SnareMed_Flam",  "SnareMed_Ruff",  "SnareMed_Off",  "SnareMed_Stick" },
    // Low
    { "SnareLow",  "SnareLow_Flam",  "SnareLow_Ruff",  "SnareLow_Off",  "SnareLow_Stick" },
    // 13"
    { "13Snare",   "13Snare_Flam",   "13Snare_Ruff",   "13Snare_off",   "13Snare_Stick" },
    // BFSD (no ruff or off)
    { "SnareBFSD", "SnareBFSD_Flam", NULL,              NULL,            "SnareBFSD_Stick" },
};

// MIDI notes 26-30 map to snare articulations 0-4
static const int SNARE_MIDI_NOTES[] = { 26, 27, 28, 29, 30 };
#define NUM_SNARE_ARTICULATIONS 5

// L Crash variant prefix mapping:
// [size][articulation: 0=hit, 1=bell, 2=choke, 3=swell]
static const char *LCRASH_PREFIXES[2][4] = {
    { "17ByzThinCrash_MainHit", "17ByzThinCrash_Bell", "17ByzThinCrash_Choke", "17ByzThinCrash_Swell" },
    { "18MedByzThinCrash",      "18MedByzThinCrash_Bell", "18MedByzThinCrash_Choke", "18MedByzThinCrash_Swell" },
};
static const int LCRASH_MIDI_NOTES[] = { 62, 63, 64, 65 };

// R Crash variant prefix mapping:
static const char *RCRASH_PREFIXES[2][4] = {
    { "20ByzThinCrash_MainHit", "20ByzThinCrash_Bell", "20ByzThinCrash_Choke", "20ByzThinCrash_Swell" },
    { "19MedByzThinCrash",      "19MedByzThinCrash_bell", "19MedByzThinCrash_Choke", "19MedByzThinCrash_Swell" },
};
static const int RCRASH_MIDI_NOTES[] = { 67, 68, 69, 70 };

void midi_map_update_variants(MidiMap *map, const SampleBank *bank,
                              int kick_size, int snare_type,
                              int tom_head[4], int china_size, int stack_type,
                              int lcrash_size, int rcrash_size) {
    // Snare: swap articulation_index based on snare_type
    if (snare_type >= 0 && snare_type < 5) {
        for (int i = 0; i < NUM_SNARE_ARTICULATIONS; i++) {
            int note = SNARE_MIDI_NOTES[i];
            MidiNoteSlot *slot = &map->slots[note];
            if (slot->num_variants == 0) continue;

            const char *prefix = SNARE_PREFIXES[snare_type][i];
            if (!prefix) continue; // BFSD has no ruff/off

            int art_idx = sample_bank_find(bank, prefix);
            if (art_idx >= 0) {
                slot->variants[0].articulation_index = art_idx;
            }
        }
    }

    // L Crash: swap prefixes based on size
    if (lcrash_size >= 0 && lcrash_size < 2) {
        for (int i = 0; i < 4; i++) {
            int note = LCRASH_MIDI_NOTES[i];
            MidiNoteSlot *slot = &map->slots[note];
            if (slot->num_variants == 0) continue;
            const char *prefix = LCRASH_PREFIXES[lcrash_size][i];
            if (!prefix) continue;
            int art_idx = sample_bank_find(bank, prefix);
            if (art_idx >= 0)
                slot->variants[0].articulation_index = art_idx;
        }
    }

    // R Crash: swap prefixes based on size
    if (rcrash_size >= 0 && rcrash_size < 2) {
        for (int i = 0; i < 4; i++) {
            int note = RCRASH_MIDI_NOTES[i];
            MidiNoteSlot *slot = &map->slots[note];
            if (slot->num_variants == 0) continue;
            const char *prefix = RCRASH_PREFIXES[rcrash_size][i];
            if (!prefix) continue;
            int art_idx = sample_bank_find(bank, prefix);
            if (art_idx >= 0)
                slot->variants[0].articulation_index = art_idx;
        }
    }

    // Kick, toms, china, stack: use active_variant index (JSON has multiple entries)
    for (int note = 0; note < MAX_MIDI_NOTES; note++) {
        MidiNoteSlot *slot = &map->slots[note];
        if (slot->num_variants <= 1) continue;

        DrumType dt = slot->variants[0].drum_type;

        switch (dt) {
        case DRUM_KICK:
            if (kick_size < slot->num_variants)
                slot->active_variant = kick_size;
            break;

        case DRUM_RACK1:
        case DRUM_RACK2:
        case DRUM_FLOOR1:
        case DRUM_FLOOR2: {
            int tom_idx = dt - DRUM_RACK1;
            int head = tom_head[tom_idx];
            if (head < slot->num_variants)
                slot->active_variant = head;
            break;
        }

        case DRUM_CHINA:
            if (china_size < slot->num_variants)
                slot->active_variant = china_size;
            break;

        case DRUM_STACK:
            if (stack_type < slot->num_variants)
                slot->active_variant = stack_type;
            break;

        default:
            break;
        }
    }
}
