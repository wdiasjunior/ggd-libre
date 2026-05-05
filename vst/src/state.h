#ifndef GGD_STATE_H
#define GGD_STATE_H

#include <clap/clap.h>
#include <stdbool.h>

struct ggd_plugin;

bool state_save(const struct ggd_plugin *plug, const clap_ostream_t *stream);
bool state_load(struct ggd_plugin *plug, const clap_istream_t *stream);

#endif
