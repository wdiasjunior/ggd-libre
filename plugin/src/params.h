#ifndef GGD_PARAMS_H
#define GGD_PARAMS_H

#include "types.h"
#include <clap/clap.h>

uint32_t params_count(void);
bool     params_get_info(uint32_t param_index, clap_param_info_t *info);
bool     params_get_value(uint32_t param_id, const void *plugin_data, double *out);
bool     params_value_to_text(uint32_t param_id, double value,
                              char *buf, uint32_t buf_size);
bool     params_text_to_value(uint32_t param_id, const char *text, double *out);

// Maps param_index (0..N-1) to param_id
uint32_t params_index_to_id(uint32_t index);
// Maps param_id back to param_index, returns -1 if invalid
int      params_id_to_index(uint32_t id);

#endif
