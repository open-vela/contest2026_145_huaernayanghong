/**
 * @file tool_registry.c
 * @brief Declarative Tool Calling & Registry Implementation
 * @author OpenVela Contest 2026 Team 145
 */

#include "tool_registry.h"
#include "event_bus.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__has_include)
#  if __has_include(<netutils/cJSON.h>)
#    include <netutils/cJSON.h>
#  elif __has_include(<cJSON/cJSON.h>)
#    include <cJSON/cJSON.h>
#  elif __has_include(<cjson/cJSON.h>)
#    include <cjson/cJSON.h>
#  else
#    include <cJSON.h>
#  endif
#else
#  include <netutils/cJSON.h>
#endif

static phoenix_tool_desc_t g_registry[PHOENIX_MAX_TOOLS];
static size_t g_tool_count = 0;
static bool g_initialized = false;

int phoenix_tool_registry_init(void)
{
    memset(g_registry, 0, sizeof(g_registry));
    g_tool_count = 0;
    g_initialized = true;
    return 0;
}

int phoenix_tool_register(const phoenix_tool_desc_t *tool)
{
    if (!g_initialized || !tool || !tool->name || !tool->execute) {
        return -1;
    }

    if (g_tool_count >= PHOENIX_MAX_TOOLS) {
        return -2; /* Registry full */
    }

    /* Check duplicate */
    for (size_t i = 0; i < g_tool_count; i++) {
        if (strcmp(g_registry[i].name, tool->name) == 0) {
            /* Update existing */
            g_registry[i] = *tool;
            return 0;
        }
    }

    g_registry[g_tool_count++] = *tool;
    printf("[PhoenixTool] Registered tool: \"%s\"\n", tool->name);
    return 0;
}

const phoenix_tool_desc_t* phoenix_tool_find(const char *name)
{
    if (!g_initialized || !name) return NULL;

    for (size_t i = 0; i < g_tool_count; i++) {
        if (strcmp(g_registry[i].name, name) == 0) {
            return &g_registry[i];
        }
    }
    return NULL;
}

int phoenix_tool_execute(const char *name, const char *args_json, char *result_out, size_t max_len)
{
    const phoenix_tool_desc_t *tool = phoenix_tool_find(name);
    if (!tool || !tool->execute) {
        if (result_out && max_len > 0) {
            snprintf(result_out, max_len, "{\"error\": \"tool not found: %s\"}", name ? name : "unknown");
        }
        return -1;
    }

    int ret = tool->execute(args_json, result_out, max_len);

    /* Broadcast tool execution event */
    phoenix_event_data_t evt;
    memset(&evt, 0, sizeof(evt));
    evt.type = PHOENIX_EVT_TOOL_TRIGGERED;
    evt.data.tool.tool_name = tool->name;
    evt.data.tool.result_summary = result_out;
    evt.data.tool.success = (ret == 0);
    phoenix_event_publish(&evt);

    return ret;
}

char* phoenix_tool_build_schema_json(void)
{
    if (!g_initialized || g_tool_count == 0) {
        return NULL;
    }

    cJSON *root_array = cJSON_CreateArray();
    if (!root_array) return NULL;

    for (size_t i = 0; i < g_tool_count; i++) {
        const phoenix_tool_desc_t *tool = &g_registry[i];
        cJSON *tool_obj = cJSON_CreateObject();
        cJSON_AddStringToObject(tool_obj, "type", "function");

        cJSON *func_obj = cJSON_CreateObject();
        cJSON_AddStringToObject(func_obj, "name", tool->name);
        cJSON_AddStringToObject(func_obj, "description", tool->description ? tool->description : "");

        cJSON *params_json = NULL;
        if (tool->parameters_schema && strlen(tool->parameters_schema) > 0) {
            params_json = cJSON_Parse(tool->parameters_schema);
        }
        if (!params_json) {
            params_json = cJSON_CreateObject();
            cJSON_AddStringToObject(params_json, "type", "object");
            cJSON_AddItemToObject(params_json, "properties", cJSON_CreateObject());
        }

        cJSON_AddItemToObject(func_obj, "parameters", params_json);
        cJSON_AddItemToObject(tool_obj, "function", func_obj);
        cJSON_AddItemToArray(root_array, tool_obj);
    }

    char *json_str = cJSON_PrintUnformatted(root_array);
    cJSON_Delete(root_array);
    return json_str;
}

size_t phoenix_tool_get_count(void)
{
    return g_tool_count;
}

void phoenix_tool_registry_deinit(void)
{
    memset(g_registry, 0, sizeof(g_registry));
    g_tool_count = 0;
    g_initialized = false;
}
