#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <dirent.h>
#include <clap/clap.h>
#include "plugin.h"

#ifdef _WIN32
#include <windows.h>
#define PATH_SEP '\\'
static void resolve_absolute(const char *path, char *out, size_t out_size) {
    if (!_fullpath(out, path, (int)out_size))
        strncpy(out, path, out_size - 1);
}
static const char *get_home_dir(void) {
    const char *p = getenv("USERPROFILE");
    if (!p) p = getenv("APPDATA");
    return p;
}
static const char *get_clap_subdir(void) { return "/Common Files/CLAP"; }
#else
#define PATH_SEP '/'
static void resolve_absolute(const char *path, char *out, size_t out_size) {
    if (!realpath(path, out))
        strncpy(out, path, out_size - 1);
}
static const char *get_home_dir(void) { return getenv("HOME"); }
static const char *get_clap_subdir(void) { return "/.clap"; }
#endif

// Find last path separator (handles both / and \ on Windows)
static char *find_last_sep(char *path) {
    char *a = strrchr(path, '/');
    char *b = strrchr(path, '\\');
    if (!a) return b;
    if (!b) return a;
    return (a > b) ? a : b;
}

static char s_plugin_path[1024] = {0};

static bool entry_init(const char *plugin_path) {
    if (plugin_path)
        strncpy(s_plugin_path, plugin_path, sizeof(s_plugin_path) - 1);
    return true;
}

static void entry_deinit(void) {}

// ---------- Plugin Factory ----------

static uint32_t factory_get_count(const struct clap_plugin_factory *factory) {
    (void)factory;
    return 1;
}

static const clap_plugin_descriptor_t *
factory_get_descriptor(const struct clap_plugin_factory *factory, uint32_t index) {
    (void)factory;
    if (index != 0) return NULL;
    extern const clap_plugin_descriptor_t *ggd_get_descriptor(void);
    return ggd_get_descriptor();
}

static const clap_plugin_t *
factory_create_plugin(const struct clap_plugin_factory *factory,
                      const clap_host_t *host, const char *plugin_id) {
    (void)factory;
    if (!clap_version_is_compatible(host->clap_version))
        return NULL;

    extern const clap_plugin_descriptor_t *ggd_get_descriptor(void);
    const clap_plugin_descriptor_t *desc = ggd_get_descriptor();
    if (strcmp(plugin_id, desc->id) != 0)
        return NULL;

    clap_plugin_t *plugin = ggd_plugin_create(host);
    if (plugin) {
        ggd_plugin_t *plug = plugin->plugin_data;

        // Resolve the plugin directory to an absolute path
        char base[1024] = {0};
        if (s_plugin_path[0]) {
            char resolved[1024] = {0};
            resolve_absolute(s_plugin_path, resolved, sizeof(resolved));
            strncpy(base, resolved, sizeof(base) - 1);

            // Strip filename if it looks like a file (has a dot in last component)
            char *last_sep = find_last_sep(base);
            if (last_sep && strchr(last_sep, '.'))
                *last_sep = '\0';
        }

        // If base is still empty, try default CLAP dir
        if (!base[0]) {
            const char *home = get_home_dir();
            if (home)
                snprintf(base, sizeof(base), "%s%s", home, get_clap_subdir());
        }

        fprintf(stderr, "ggd-libre: plugin_path='%s' resolved base='%s'\n",
                s_plugin_path, base);

        if (base[0]) {
            const char *candidates[] = {
                "%s/GGD Matt Halpern Signature Pack/wav",
                "%s/output/halpern/wav",
                "%s/../output/halpern/wav",
                "%s/wav",
                NULL
            };
            char try_path[1024];
            for (int i = 0; candidates[i]; i++) {
                snprintf(try_path, sizeof(try_path), candidates[i], base);
                DIR *d = opendir(try_path);
                if (d) {
                    closedir(d);
                    char abs[1024] = {0};
                    resolve_absolute(try_path, abs, sizeof(abs));
                    strncpy(plug->samples_path, abs, sizeof(plug->samples_path) - 1);
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
