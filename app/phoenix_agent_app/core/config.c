/**
 * @file config.c
 * @brief Generic Key-Value Configuration Subsystem Implementation
 * @author OpenVela Contest 2026 Team 145
 */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_CONFIG_ENTRIES 32
#define MAX_KEY_LEN        32
#define MAX_VAL_LEN        128

typedef struct {
    char key[MAX_KEY_LEN];
    char val[MAX_VAL_LEN];
    bool used;
} config_entry_t;

static config_entry_t g_entries[MAX_CONFIG_ENTRIES];
static char g_config_path[256] = {0};
static bool g_config_initialized = false;

static config_entry_t* find_entry(const char *key)
{
    if (!key) return NULL;
    for (size_t i = 0; i < MAX_CONFIG_ENTRIES; i++) {
        if (g_entries[i].used && strcmp(g_entries[i].key, key) == 0) {
            return &g_entries[i];
        }
    }
    return NULL;
}

static config_entry_t* alloc_entry(const char *key)
{
    config_entry_t *existing = find_entry(key);
    if (existing) return existing;

    for (size_t i = 0; i < MAX_CONFIG_ENTRIES; i++) {
        if (!g_entries[i].used) {
            g_entries[i].used = true;
            snprintf(g_entries[i].key, sizeof(g_entries[i].key), "%s", key);
            g_entries[i].val[0] = '\0';
            return &g_entries[i];
        }
    }
    return NULL;
}

int phoenix_config_init(const char *storage_dir)
{
    memset(g_entries, 0, sizeof(g_entries));
    if (storage_dir && strlen(storage_dir) > 0) {
        snprintf(g_config_path, sizeof(g_config_path), "%s/phoenix_config.txt", storage_dir);
    } else {
        snprintf(g_config_path, sizeof(g_config_path), "/tmp/phoenix_config.txt");
    }

    /* Set sensible defaults */
    phoenix_config_set_int(PHOENIX_CFG_VOLUME, 80);
    phoenix_config_set_int(PHOENIX_CFG_BRIGHTNESS, 90);
    phoenix_config_set_int(PHOENIX_CFG_PROACTIVE_EN, 1);
    phoenix_config_set_int(PHOENIX_CFG_PROACTIVE_TIMEOUT, 2700);
    phoenix_config_set_str(PHOENIX_CFG_BACKEND, "mock");
    phoenix_config_set_str(PHOENIX_CFG_MODEL, "deepseek-chat");

    /* Try loading existing config file */
    FILE *fp = fopen(g_config_path, "r");
    if (fp) {
        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            char k[MAX_KEY_LEN] = {0};
            char v[MAX_VAL_LEN] = {0};
            if (sscanf(line, "%31[^=]=%127[^\r\n]", k, v) == 2) {
                phoenix_config_set_str(k, v);
            }
        }
        fclose(fp);
    }

    g_config_initialized = true;
    printf("[PhoenixConfig] ⚙️ Configuration subsystem initialized. Store: %s\n", g_config_path);
    return 0;
}

int phoenix_config_get_int(const char *key, int default_val)
{
    if (!g_config_initialized || !key) return default_val;
    config_entry_t *e = find_entry(key);
    if (!e || strlen(e->val) == 0) return default_val;
    return atoi(e->val);
}

int phoenix_config_set_int(const char *key, int val)
{
    if (!key) return -1;
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", val);
    return phoenix_config_set_str(key, buf);
}

const char* phoenix_config_get_str(const char *key, const char *default_val, char *out_buf, size_t buf_sz)
{
    if (!out_buf || buf_sz == 0) return default_val;
    if (!g_config_initialized || !key) {
        if (default_val) snprintf(out_buf, buf_sz, "%s", default_val);
        else out_buf[0] = '\0';
        return out_buf;
    }

    config_entry_t *e = find_entry(key);
    if (!e || strlen(e->val) == 0) {
        if (default_val) snprintf(out_buf, buf_sz, "%s", default_val);
        else out_buf[0] = '\0';
        return out_buf;
    }

    snprintf(out_buf, buf_sz, "%s", e->val);
    return out_buf;
}

int phoenix_config_set_str(const char *key, const char *val)
{
    if (!key) return -1;
    config_entry_t *e = alloc_entry(key);
    if (!e) return -2; /* Capacity full */

    if (val) {
        snprintf(e->val, sizeof(e->val), "%s", val);
    } else {
        e->val[0] = '\0';
    }
    return 0;
}

int phoenix_config_save(void)
{
    if (!g_config_initialized || strlen(g_config_path) == 0) return -1;
    FILE *fp = fopen(g_config_path, "w");
    if (!fp) return -2;

    for (size_t i = 0; i < MAX_CONFIG_ENTRIES; i++) {
        if (g_entries[i].used) {
            fprintf(fp, "%s=%s\n", g_entries[i].key, g_entries[i].val);
        }
    }
    fclose(fp);
    return 0;
}

void phoenix_config_deinit(void)
{
    if (g_config_initialized) {
        phoenix_config_save();
    }
    memset(g_entries, 0, sizeof(g_entries));
    g_config_initialized = false;
}
