#ifndef GGD_PARAMS_H
#define GGD_PARAMS_H

#include "types.h"
#include "library.h"
#include <clap/clap.h>

typedef enum {
    PK_NONE = 0,
    PK_MASTER,
    PK_MIDI_MAP,
    PK_TAB_MASTER,   // index = tab
    PK_CHANNEL,      // index = channel, offset = PARAM_CH_*
    PK_SELECTOR,     // index = selector
} ParamKind;

typedef struct {
    ParamKind kind;
    int       lib;
    int       index;
    int       offset;
} ParamRef;

// Build the id table. Call once before using the others (plug_init).
void     params_init(void);
uint32_t params_count(void);
uint32_t params_index_to_id(uint32_t index);
bool     params_resolve(uint32_t id, ParamRef *out);
bool     params_get_info(uint32_t param_index, clap_param_info_t *info);
bool     params_value_to_text(uint32_t param_id, double value, char *buf, uint32_t buf_size);
bool     params_text_to_value(uint32_t param_id, const char *text, double *out);

#endif
