#include "params.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Global params, then each library's block: tab masters, channels, selectors.
#define MAX_PARAMS (2 + LIB_COUNT * (TAB_COUNT + MAX_LIB_CHANNELS * PARAM_CH_COUNT + MAX_SELECTORS))

static uint32_t s_ids[MAX_PARAMS];
static uint32_t s_count;

static const char *TAB_MASTER_NAMES[TAB_COUNT] = {
    "Kick Master", "Snare Master", "Toms Master", "Cymbals Master"
};
static const char *TAB_MODULE_NAMES[TAB_COUNT] = { "Kick", "Snare", "Toms", "Cymbals" };
static const char *CH_PARAM_NAMES[PARAM_CH_COUNT] = {
    "Gain", "Pan", "Mute", "Solo", "Phase", "Stereo"
};

void params_init(void) {
    if (s_count) return;
    uint32_t n = 0;
    s_ids[n++] = PARAM_MASTER_GAIN;
    s_ids[n++] = PARAM_MIDI_MAP_MODE;
    for (int l = 0; l < LIB_COUNT; l++) {
        const LibraryDef *d = library_def(l);
        for (int t = 0; t < TAB_COUNT; t++)
            s_ids[n++] = lib_param_tab_master(d, t);
        for (int ch = 0; ch < d->num_channels; ch++)
            for (int p = 0; p < PARAM_CH_COUNT; p++)
                s_ids[n++] = lib_param_channel(d, ch, p);
        for (int s = 0; s < d->num_selectors; s++)
            s_ids[n++] = d->selectors[s].param_id;
    }
    s_count = n;
}

uint32_t params_count(void) {
    return s_count;
}

uint32_t params_index_to_id(uint32_t index) {
    return index < s_count ? s_ids[index] : CLAP_INVALID_ID;
}

bool params_resolve(uint32_t id, ParamRef *out) {
    memset(out, 0, sizeof(*out));
    if (id == PARAM_MASTER_GAIN) { out->kind = PK_MASTER; return true; }
    if (id == PARAM_MIDI_MAP_MODE) { out->kind = PK_MIDI_MAP; return true; }

    for (int l = 0; l < LIB_COUNT; l++) {
        const LibraryDef *d = library_def(l);
        out->lib = l;
        // Selectors first: Halpern's 306-309 sit inside its channel id range
        // (at offsets 6-9, which no channel param uses).
        for (int s = 0; s < d->num_selectors; s++) {
            if (d->selectors[s].param_id == id) {
                out->kind = PK_SELECTOR; out->index = s;
                return true;
            }
        }
        uint32_t tm = lib_param_tab_master(d, 0);
        if (id >= tm && id < tm + TAB_COUNT) {
            out->kind = PK_TAB_MASTER; out->index = (int)(id - tm);
            return true;
        }
        uint32_t cb = lib_param_channel(d, 0, 0);
        if (id >= cb && id < cb + (uint32_t)d->num_channels * 10) {
            uint32_t rel = id - cb;
            if (rel % 10 < PARAM_CH_COUNT) {
                out->kind = PK_CHANNEL; out->index = (int)(rel / 10); out->offset = (int)(rel % 10);
                return true;
            }
        }
    }
    out->kind = PK_NONE;
    return false;
}

static void name_with_prefix(char *dst, size_t size, const LibraryDef *d, const char *rest) {
    if (d->param_prefix && d->param_prefix[0])
        snprintf(dst, size, "%s %s", d->param_prefix, rest);
    else
        snprintf(dst, size, "%s", rest);
}

static const char *lib_module(const LibraryDef *d) {
    return (d->param_prefix && d->param_prefix[0]) ? d->param_prefix : "Halpern";
}

bool params_get_info(uint32_t param_index, clap_param_info_t *info) {
    if (param_index >= s_count) return false;
    uint32_t id = s_ids[param_index];
    ParamRef ref;
    if (!params_resolve(id, &ref)) return false;

    memset(info, 0, sizeof(*info));
    info->id = id;
    info->cookie = NULL;
    info->flags = CLAP_PARAM_IS_AUTOMATABLE;
    const LibraryDef *d = library_def(ref.lib);
    char buf[CLAP_NAME_SIZE];

    switch (ref.kind) {
    case PK_MASTER:
        snprintf(info->name, CLAP_NAME_SIZE, "Master Gain");
        snprintf(info->module, CLAP_PATH_SIZE, "Master");
        info->min_value = -80.0;
        info->max_value = 12.0;
        return true;

    case PK_MIDI_MAP:
        snprintf(info->name, CLAP_NAME_SIZE, "MIDI Map");
        snprintf(info->module, CLAP_PATH_SIZE, "Master");
        info->max_value = (double)(MIDIMAP_MODE_COUNT - 1);
        info->flags |= CLAP_PARAM_IS_STEPPED;
        return true;

    case PK_TAB_MASTER:
        name_with_prefix(info->name, CLAP_NAME_SIZE, d, TAB_MASTER_NAMES[ref.index]);
        snprintf(info->module, CLAP_PATH_SIZE, "%s/Master", lib_module(d));
        info->min_value = -80.0;
        info->max_value = 12.0;
        return true;

    case PK_CHANNEL: {
        const LibChannel *c = &d->channels[ref.index];
        snprintf(buf, sizeof(buf), "%s %s", c->name, CH_PARAM_NAMES[ref.offset]);
        name_with_prefix(info->name, CLAP_NAME_SIZE, d, buf);
        snprintf(info->module, CLAP_PATH_SIZE, "%s/%s/%s", lib_module(d),
                 TAB_MODULE_NAMES[c->tab], c->name);
        switch (ref.offset) {
        case PARAM_CH_GAIN:
            info->min_value = -80.0; info->max_value = 12.0;
            break;
        case PARAM_CH_PAN:
            info->min_value = -1.0; info->max_value = 1.0;
            break;
        case PARAM_CH_STEREO:
            info->max_value = 1.0; info->default_value = 1.0;
            info->flags |= CLAP_PARAM_IS_STEPPED;
            break;
        default:   // mute, solo, phase
            info->max_value = 1.0;
            info->flags |= CLAP_PARAM_IS_STEPPED;
            break;
        }
        return true;
    }

    case PK_SELECTOR: {
        const LibSelector *s = &d->selectors[ref.index];
        name_with_prefix(info->name, CLAP_NAME_SIZE, d, s->name);
        snprintf(info->module, CLAP_PATH_SIZE, "%s/Variants", lib_module(d));
        info->max_value = (double)(s->num_options - 1);
        info->default_value = (double)s->default_option;
        info->flags |= CLAP_PARAM_IS_STEPPED;
        return true;
    }

    default:
        return false;
    }
}

static void db_text(double value, char *buf, uint32_t buf_size) {
    if (value <= -80.0) snprintf(buf, buf_size, "-inf dB");
    else snprintf(buf, buf_size, "%.1f dB", value);
}

bool params_value_to_text(uint32_t param_id, double value, char *buf, uint32_t buf_size) {
    ParamRef ref;
    if (!params_resolve(param_id, &ref)) return false;
    int vi = (int)(value + 0.5);

    switch (ref.kind) {
    case PK_MASTER:
    case PK_TAB_MASTER:
        db_text(value, buf, buf_size);
        return true;
    case PK_MIDI_MAP:
        if (vi < 0 || vi >= MIDIMAP_MODE_COUNT) return false;
        snprintf(buf, buf_size, "%s", MIDI_MAP_MODE_NAMES[vi]);
        return true;
    case PK_CHANNEL:
        switch (ref.offset) {
        case PARAM_CH_GAIN: db_text(value, buf, buf_size); return true;
        case PARAM_CH_PAN:
            if (value < -0.01)     snprintf(buf, buf_size, "%.0f%% L", -value * 100);
            else if (value > 0.01) snprintf(buf, buf_size, "%.0f%% R", value * 100);
            else                   snprintf(buf, buf_size, "C");
            return true;
        case PARAM_CH_MUTE:
        case PARAM_CH_SOLO:   snprintf(buf, buf_size, "%s", value > 0.5 ? "On" : "Off"); return true;
        case PARAM_CH_PHASE:  snprintf(buf, buf_size, "%s", value > 0.5 ? "Inverted" : "Normal"); return true;
        case PARAM_CH_STEREO: snprintf(buf, buf_size, "%s", value > 0.5 ? "Stereo" : "Mono"); return true;
        }
        return false;
    case PK_SELECTOR: {
        const LibSelector *s = &library_def(ref.lib)->selectors[ref.index];
        if (vi < 0 || vi >= s->num_options) return false;
        snprintf(buf, buf_size, "%s", s->options[vi]);
        return true;
    }
    default:
        return false;
    }
}

bool params_text_to_value(uint32_t param_id, const char *text, double *out) {
    ParamRef ref;
    if (params_resolve(param_id, &ref) && ref.kind == PK_SELECTOR) {
        const LibSelector *s = &library_def(ref.lib)->selectors[ref.index];
        for (int o = 0; o < s->num_options; o++)
            if (strcmp(text, s->options[o]) == 0) { *out = o; return true; }
    }
    char *end;
    double val = strtod(text, &end);
    if (end != text) {
        *out = val;
        return true;
    }
    return false;
}
