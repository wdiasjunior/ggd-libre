#include "params.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Build a table of all param IDs in order
static uint32_t s_param_ids[TOTAL_PARAMS];
static bool s_ids_built = false;

static void build_id_table(void) {
    if (s_ids_built) return;
    int idx = 0;
    s_param_ids[idx++] = PARAM_MASTER_GAIN;
    for (int ch = 0; ch < DRUM_CHANNEL_COUNT; ch++) {
        for (int p = 0; p < PARAM_CH_COUNT; p++) {
            s_param_ids[idx++] = param_channel_id((DrumChannel)ch, p);
        }
    }
    for (int v = 0; v < PARAM_VAR_COUNT; v++) {
        s_param_ids[idx++] = PARAM_VAR_BASE + v;
    }
    s_ids_built = true;
}

uint32_t params_count(void) {
    return TOTAL_PARAMS;
}

uint32_t params_index_to_id(uint32_t index) {
    build_id_table();
    if (index >= TOTAL_PARAMS) return 0;
    return s_param_ids[index];
}

int params_id_to_index(uint32_t id) {
    build_id_table();
    for (uint32_t i = 0; i < TOTAL_PARAMS; i++) {
        if (s_param_ids[i] == id) return (int)i;
    }
    return -1;
}

static const char *variant_names_kick[] = {"22x16", "22x20"};
static const char *variant_names_snare[] = {"High", "Med", "Low", "13\"", "BFSD"};
static const char *variant_names_tom_head[] = {"Clear", "Coated"};
static const char *variant_names_china[] = {"Default", "18\""};
static const char *variant_names_stack[] = {"Default", "Mini"};

bool params_get_info(uint32_t param_index, clap_param_info_t *info) {
    build_id_table();
    if (param_index >= TOTAL_PARAMS) return false;

    uint32_t id = s_param_ids[param_index];
    memset(info, 0, sizeof(*info));
    info->id = id;
    info->cookie = NULL;

    if (id == PARAM_MASTER_GAIN) {
        strncpy(info->name, "Master Gain", CLAP_NAME_SIZE);
        strncpy(info->module, "Master", CLAP_PATH_SIZE);
        info->min_value = -80.0;
        info->max_value = 12.0;
        info->default_value = 0.0;
        info->flags = CLAP_PARAM_IS_AUTOMATABLE;
        return true;
    }

    DrumChannel ch;
    int offset;
    if (param_is_channel(id, &ch, &offset)) {
        const char *ch_name = DRUM_CHANNEL_NAMES[ch];
        snprintf(info->module, CLAP_PATH_SIZE, "Channels/%s", ch_name);

        switch (offset) {
        case PARAM_CH_GAIN:
            snprintf(info->name, CLAP_NAME_SIZE, "%s Gain", ch_name);
            info->min_value = -80.0;
            info->max_value = 12.0;
            info->default_value = 0.0;
            info->flags = CLAP_PARAM_IS_AUTOMATABLE;
            break;
        case PARAM_CH_PAN:
            snprintf(info->name, CLAP_NAME_SIZE, "%s Pan", ch_name);
            info->min_value = -1.0;
            info->max_value = 1.0;
            info->default_value = 0.0;
            info->flags = CLAP_PARAM_IS_AUTOMATABLE;
            break;
        case PARAM_CH_MUTE:
            snprintf(info->name, CLAP_NAME_SIZE, "%s Mute", ch_name);
            info->min_value = 0.0;
            info->max_value = 1.0;
            info->default_value = 0.0;
            info->flags = CLAP_PARAM_IS_AUTOMATABLE | CLAP_PARAM_IS_STEPPED;
            break;
        case PARAM_CH_SOLO:
            snprintf(info->name, CLAP_NAME_SIZE, "%s Solo", ch_name);
            info->min_value = 0.0;
            info->max_value = 1.0;
            info->default_value = 0.0;
            info->flags = CLAP_PARAM_IS_AUTOMATABLE | CLAP_PARAM_IS_STEPPED;
            break;
        case PARAM_CH_PHASE:
            snprintf(info->name, CLAP_NAME_SIZE, "%s Phase", ch_name);
            info->min_value = 0.0;
            info->max_value = 1.0;
            info->default_value = 0.0;
            info->flags = CLAP_PARAM_IS_AUTOMATABLE | CLAP_PARAM_IS_STEPPED;
            break;
        case PARAM_CH_STEREO:
            snprintf(info->name, CLAP_NAME_SIZE, "%s Stereo", ch_name);
            info->min_value = 0.0;
            info->max_value = 1.0;
            info->default_value = 1.0;
            info->flags = CLAP_PARAM_IS_AUTOMATABLE | CLAP_PARAM_IS_STEPPED;
            break;
        default:
            return false;
        }
        return true;
    }

    // Variant selectors
    if (id >= PARAM_VAR_BASE && id < PARAM_VAR_BASE + PARAM_VAR_COUNT) {
        info->flags = CLAP_PARAM_IS_AUTOMATABLE | CLAP_PARAM_IS_STEPPED;
        info->default_value = 0.0;
        strncpy(info->module, "Variants", CLAP_PATH_SIZE);

        switch (id) {
        case PARAM_VAR_KICK_SIZE:
            strncpy(info->name, "Kick Size", CLAP_NAME_SIZE);
            info->max_value = 1.0;
            break;
        case PARAM_VAR_SNARE_TYPE:
            strncpy(info->name, "Snare Type", CLAP_NAME_SIZE);
            info->max_value = 4.0;
            break;
        case PARAM_VAR_TOM1_HEAD:
        case PARAM_VAR_TOM2_HEAD:
        case PARAM_VAR_TOM3_HEAD:
        case PARAM_VAR_TOM4_HEAD:
            snprintf(info->name, CLAP_NAME_SIZE, "Tom %d Head", (int)(id - PARAM_VAR_TOM1_HEAD + 1));
            info->max_value = 1.0;
            break;
        case PARAM_VAR_CHINA_SIZE:
            strncpy(info->name, "China Size", CLAP_NAME_SIZE);
            info->max_value = 1.0;
            break;
        case PARAM_VAR_STACK_TYPE:
            strncpy(info->name, "Stack Type", CLAP_NAME_SIZE);
            info->max_value = 1.0;
            break;
        }
        return true;
    }

    return false;
}

bool params_value_to_text(uint32_t param_id, double value,
                          char *buf, uint32_t buf_size) {
    if (param_id == PARAM_MASTER_GAIN) {
        if (value <= -80.0)
            snprintf(buf, buf_size, "-inf dB");
        else
            snprintf(buf, buf_size, "%.1f dB", value);
        return true;
    }

    DrumChannel ch;
    int offset;
    if (param_is_channel(param_id, &ch, &offset)) {
        switch (offset) {
        case PARAM_CH_GAIN:
            if (value <= -80.0)
                snprintf(buf, buf_size, "-inf dB");
            else
                snprintf(buf, buf_size, "%.1f dB", value);
            return true;
        case PARAM_CH_PAN:
            if (value < -0.01)
                snprintf(buf, buf_size, "%.0f%% L", -value * 100);
            else if (value > 0.01)
                snprintf(buf, buf_size, "%.0f%% R", value * 100);
            else
                snprintf(buf, buf_size, "C");
            return true;
        case PARAM_CH_MUTE:
            snprintf(buf, buf_size, "%s", value > 0.5 ? "On" : "Off");
            return true;
        case PARAM_CH_SOLO:
            snprintf(buf, buf_size, "%s", value > 0.5 ? "On" : "Off");
            return true;
        case PARAM_CH_PHASE:
            snprintf(buf, buf_size, "%s", value > 0.5 ? "Inverted" : "Normal");
            return true;
        case PARAM_CH_STEREO:
            snprintf(buf, buf_size, "%s", value > 0.5 ? "Stereo" : "Mono");
            return true;
        }
    }

    // Variant selectors
    int vi = (int)(value + 0.5);
    switch (param_id) {
    case PARAM_VAR_KICK_SIZE:
        if (vi >= 0 && vi <= 1)
            snprintf(buf, buf_size, "%s", variant_names_kick[vi]);
        return true;
    case PARAM_VAR_SNARE_TYPE:
        if (vi >= 0 && vi <= 4)
            snprintf(buf, buf_size, "%s", variant_names_snare[vi]);
        return true;
    case PARAM_VAR_TOM1_HEAD:
    case PARAM_VAR_TOM2_HEAD:
    case PARAM_VAR_TOM3_HEAD:
    case PARAM_VAR_TOM4_HEAD:
        if (vi >= 0 && vi <= 1)
            snprintf(buf, buf_size, "%s", variant_names_tom_head[vi]);
        return true;
    case PARAM_VAR_CHINA_SIZE:
        if (vi >= 0 && vi <= 1)
            snprintf(buf, buf_size, "%s", variant_names_china[vi]);
        return true;
    case PARAM_VAR_STACK_TYPE:
        if (vi >= 0 && vi <= 1)
            snprintf(buf, buf_size, "%s", variant_names_stack[vi]);
        return true;
    }

    return false;
}

bool params_text_to_value(uint32_t param_id, const char *text, double *out) {
    // Simple: try to parse as number
    char *end;
    double val = strtod(text, &end);
    if (end != text) {
        *out = val;
        return true;
    }
    return false;
}
