#include "state.h"
#include "plugin.h"
#include <string.h>

#define STATE_MAGIC 0x47474431  // "GGD1"
#define STATE_VERSION 4

typedef struct {
    uint32_t magic;
    uint32_t version;
    float    master_gain_db;
    float    tab_master_db[TAB_COUNT];
    int32_t  kick_size;
    int32_t  snare_type;
    int32_t  tom_head[4];
    int32_t  china_size;
    int32_t  stack_type;
    int32_t  lcrash_size;
    int32_t  rcrash_size;
    struct {
        float gain_db;
        float pan;
        uint8_t mute;
        uint8_t solo;
        uint8_t phase_invert;
        uint8_t stereo_mode;
    } channels[MIXER_CHANNEL_COUNT];
} StateData;

static int64_t stream_write_all(const clap_ostream_t *stream, const void *buf, uint64_t size) {
    const uint8_t *p = buf;
    uint64_t written = 0;
    while (written < size) {
        int64_t n = stream->write(stream, p + written, size - written);
        if (n <= 0) return -1;
        written += n;
    }
    return written;
}

static int64_t stream_read_all(const clap_istream_t *stream, void *buf, uint64_t size) {
    uint8_t *p = buf;
    uint64_t total = 0;
    while (total < size) {
        int64_t n = stream->read(stream, p + total, size - total);
        if (n <= 0) return -1;
        total += n;
    }
    return total;
}

bool state_save(const struct ggd_plugin *plug, const clap_ostream_t *stream) {
    StateData state;
    memset(&state, 0, sizeof(state));
    state.magic = STATE_MAGIC;
    state.version = STATE_VERSION;
    state.master_gain_db = plug->engine.master_gain_db;
    for (int t = 0; t < TAB_COUNT; t++)
        state.tab_master_db[t] = plug->engine.tab_master_db[t];
    state.kick_size = plug->kick_size;
    state.snare_type = plug->snare_type;
    for (int i = 0; i < 4; i++) state.tom_head[i] = plug->tom_head[i];
    state.china_size = plug->china_size;
    state.stack_type = plug->stack_type;
    state.lcrash_size = plug->lcrash_size;
    state.rcrash_size = plug->rcrash_size;

    for (int i = 0; i < MIXER_CHANNEL_COUNT; i++) {
        const ChannelParams *ch = &plug->engine.channels[i];
        state.channels[i].gain_db = ch->gain_db;
        state.channels[i].pan = ch->pan;
        state.channels[i].mute = ch->mute ? 1 : 0;
        state.channels[i].solo = ch->solo ? 1 : 0;
        state.channels[i].phase_invert = ch->phase_invert ? 1 : 0;
        state.channels[i].stereo_mode = ch->stereo_mode ? 1 : 0;
    }

    return stream_write_all(stream, &state, sizeof(state)) > 0;
}

bool state_load(struct ggd_plugin *plug, const clap_istream_t *stream) {
    StateData state;
    if (stream_read_all(stream, &state, sizeof(state)) < 0)
        return false;

    if (state.magic != STATE_MAGIC || state.version != STATE_VERSION)
        return false;

    plug->engine.master_gain_db = state.master_gain_db;
    engine_update_master(&plug->engine);
    for (int t = 0; t < TAB_COUNT; t++) {
        plug->engine.tab_master_db[t] = state.tab_master_db[t];
        engine_update_tab_master(&plug->engine, (GuiTab)t);
    }

    plug->kick_size = state.kick_size;
    plug->snare_type = state.snare_type;
    for (int i = 0; i < 4; i++) plug->tom_head[i] = state.tom_head[i];
    plug->china_size = state.china_size;
    plug->stack_type = state.stack_type;
    plug->lcrash_size = state.lcrash_size;
    plug->rcrash_size = state.rcrash_size;

    for (int i = 0; i < MIXER_CHANNEL_COUNT; i++) {
        ChannelParams *ch = &plug->engine.channels[i];
        ch->gain_db = state.channels[i].gain_db;
        ch->pan = state.channels[i].pan;
        ch->mute = state.channels[i].mute != 0;
        ch->solo = state.channels[i].solo != 0;
        ch->phase_invert = state.channels[i].phase_invert != 0;
        ch->stereo_mode = state.channels[i].stereo_mode != 0;
        engine_update_channel(&plug->engine, (MixerChannel)i);
    }
    engine_update_solo_state(&plug->engine);

    midi_map_update_variants(&plug->midi_map, &plug->bank,
                             plug->kick_size, plug->snare_type,
                             plug->tom_head, plug->china_size, plug->stack_type,
                             plug->lcrash_size, plug->rcrash_size);
    return true;
}
