#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <dirent.h>
#include <clap/clap.h>
#include "plugin.h"

static char s_plugin_path[1024] = {0};

static bool entry_init(const char *plugin_path) {
    if (plugin_path)
        strncpy(s_plugin_path, plugin_path, sizeof(s_plugin_path) - 1);
    return true;
}

static void entry_deinit(void) {}

// ---------- Plugin Factory ----------

static uint32_t factory_get_count(const struct clap_plugin_factory *factory) {
    return 1;
}

static const clap_plugin_descriptor_t *
factory_get_descriptor(const struct clap_plugin_factory *factory, uint32_t index) {
    if (index != 0) return NULL;
    // We need access to the descriptor; it's defined in plugin.c
    // Create a temporary plugin to get its descriptor
    // Actually, we'll just declare it extern or duplicate the descriptor.
    // For simplicity, create the plugin, grab the desc, and rely on the host to call create.
    // Better approach: expose the descriptor from plugin.c
    extern const clap_plugin_descriptor_t *ggd_get_descriptor(void);
    return ggd_get_descriptor();
}

static const clap_plugin_t *
factory_create_plugin(const struct clap_plugin_factory *factory,
                      const clap_host_t *host, const char *plugin_id) {
    if (!clap_version_is_compatible(host->clap_version))
        return NULL;

    extern const clap_plugin_descriptor_t *ggd_get_descriptor(void);
    const clap_plugin_descriptor_t *desc = ggd_get_descriptor();
    if (strcmp(plugin_id, desc->id) != 0)
        return NULL;

    clap_plugin_t *plugin = ggd_plugin_create(host);
    if (plugin) {
        // Set the samples path based on the plugin path
        ggd_plugin_t *plug = plugin->plugin_data;
        // Resolve the plugin directory to an absolute path
        char base[1024] = {0};
        if (s_plugin_path[0]) {
            // plugin_path may be a file path or a directory
            char resolved[1024];
            if (realpath(s_plugin_path, resolved)) {
                strncpy(base, resolved, sizeof(base) - 1);
            } else {
                strncpy(base, s_plugin_path, sizeof(base) - 1);
            }
            // Strip filename if it looks like a file (has a dot in last component)
            char *last_slash = strrchr(base, '/');
            if (last_slash) {
                if (strchr(last_slash, '.'))
                    *last_slash = '\0';  // strip filename
            }
        }

        // If base is still empty, try ~/.clap
        if (!base[0]) {
            const char *home = getenv("HOME");
            if (home)
                snprintf(base, sizeof(base), "%s/.clap", home);
        }

        fprintf(stderr, "ggd-libre: plugin_path='%s' resolved base='%s'\n",
                s_plugin_path, base);

        if (base[0]) {
            const char *candidates[] = {
                "%s/output/wav",       // ~/.clap/output/wav
                "%s/../output/wav",    // <project>/output/wav (dev layout)
                "%s/wav",              // ~/.clap/wav (flat)
                NULL
            };
            char try_path[1024];
            for (int i = 0; candidates[i]; i++) {
                snprintf(try_path, sizeof(try_path), candidates[i], base);
                DIR *d = opendir(try_path);
                if (d) {
                    closedir(d);
                    // Store the realpath so logs show a clean absolute path
                    char abs[1024];
                    if (realpath(try_path, abs))
                        strncpy(plug->samples_path, abs, sizeof(plug->samples_path) - 1);
                    else
                        strncpy(plug->samples_path, try_path, sizeof(plug->samples_path) - 1);
                    fprintf(stderr, "ggd-libre: found samples at %s\n", plug->samples_path);
                    break;
                }
            }
        }

        // Environment variable overrides auto-detection
        const char *env = getenv("GGD_SAMPLES_PATH");
        if (env && env[0])
            strncpy(plug->samples_path, env, sizeof(plug->samples_path) - 1);
    }

    return plugin;
}

static const clap_plugin_factory_t s_factory = {
    .get_plugin_count = factory_get_count,
    .get_plugin_descriptor = factory_get_descriptor,
    .create_plugin = factory_create_plugin,
};

// ---------- Entry ----------

static const void *entry_get_factory(const char *factory_id) {
    if (!strcmp(factory_id, CLAP_PLUGIN_FACTORY_ID))
        return &s_factory;
    return NULL;
}

CLAP_EXPORT const clap_plugin_entry_t clap_entry = {
    .clap_version = CLAP_VERSION_INIT,
    .init = entry_init,
    .deinit = entry_deinit,
    .get_factory = entry_get_factory,
};
