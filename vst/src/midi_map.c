#include "midi_map.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Drum channel assignment based on MIDI note ranges and sample prefixes
static DrumChannel classify_drum_channel(int midi_note, const char *prefix) {
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
    // Hi-hat: all notes 43-58 belong to hi-hat choke group
    if (midi_note >= 43 && midi_note <= 58) return CHOKE_HIHAT;
    // Crash chokes
    if (midi_note == 64) return CHOKE_LCRASH;
    if (midi_note >= 62 && midi_note <= 65) return CHOKE_LCRASH;
    if (midi_note == 69) return CHOKE_RCRASH;
    if (midi_note >= 67 && midi_note <= 70) return CHOKE_RCRASH;
    if (midi_note == 77) return CHOKE_CHINA;
    if (midi_note == 76) return CHOKE_CHINA;
    return CHOKE_NONE;
}

static bool is_choke_trigger(int midi_note) {
    // Closed hi-hat chokes open hi-hat
    // Notes 43-48 are tight/closed, they choke 49-58 (open)
    if (midi_note >= 43 && midi_note <= 48) return true;
    // Explicit choke articulations
    if (midi_note == 64) return true;  // L Crash Choke
    if (midi_note == 69) return true;  // R Crash Choke
    if (midi_note == 77) return true;  // China Choke
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
            var->drum_channel = classify_drum_channel(midi_note, prefix_item->valuestring);
            var->choke_group = get_choke_group(midi_note);
            var->is_choke_trigger = is_choke_trigger(midi_note);
        }
    }

    cJSON_Delete(root);

    // Count mapped notes
    int mapped = 0;
    for (int i = 0; i < MAX_MIDI_NOTES; i++) {
        if (map->slots[i].num_variants > 0) mapped++;
    }
    fprintf(stderr, "ggd-libre: %d MIDI notes mapped\n", mapped);
    return mapped > 0;
}

void midi_map_update_variants(MidiMap *map, const SampleBank *bank,
                              int kick_size, int snare_type,
                              int tom_head[4], int china_size, int stack_type) {
    // For notes with multiple variants, set active_variant based on selectors.
    // The variant ordering in midi_map.json follows a known pattern per drum type.

    for (int note = 0; note < MAX_MIDI_NOTES; note++) {
        MidiNoteSlot *slot = &map->slots[note];
        if (slot->num_variants <= 1) continue;

        DrumChannel ch = slot->variants[0].drum_channel;

        switch (ch) {
        case DRUM_KICK:
            // Variants: [Kick22x16*, Kick22x20*, ...]
            // kick_size: 0=22x16, 1=22x20
            // The JSON has Kick22x16_MainHit at 0, Kick22x20_MainHit at 1,
            // then Kick22x16 (room mics) at 2, Kick22x20 (room mics) at 3.
            // We want to select the pair matching kick_size.
            // For now, just pick first matching size.
            if (kick_size < slot->num_variants)
                slot->active_variant = kick_size;
            break;

        case DRUM_RACK1:
        case DRUM_RACK2:
        case DRUM_FLOOR1:
        case DRUM_FLOOR2: {
            int tom_idx = ch - DRUM_RACK1;
            int head = tom_head[tom_idx];
            // Variants: [ClearTomXX, CoatedTomXX]
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
