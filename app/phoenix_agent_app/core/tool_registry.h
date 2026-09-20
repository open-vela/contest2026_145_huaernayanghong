/**
 * @file tool_registry.h
 * @brief Declarative Tool Calling & Function Registration for Phoenix Agent
 * @author OpenVela Contest 2026 Team 145
 */

#ifndef PHOENIX_TOOL_REGISTRY_H
#define PHOENIX_TOOL_REGISTRY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>

#define PHOENIX_MAX_TOOLS 16

/**
 * @brief Tool Handler Callback Prototype
 * @param args_json JSON string of function arguments passed by LLM
 * @param result_out Buffer to store the execution result string/JSON
 * @param max_len Size of result_out buffer
 * @return 0 on success, negative error code on failure
 */
typedef int (*phoenix_tool_handler_t)(const char *args_json, char *result_out, size_t max_len);

/**
 * @brief Declarative Tool Descriptor
 */
typedef struct {
    const char *name;               /**< Tool identifier, e.g. "knock_wooden_fish" */
    const char *description;        /**< Clear description for LLM reasoning */
    const char *parameters_schema;  /**< JSON Schema for arguments, or "{}" */
    phoenix_tool_handler_t execute; /**< Function implementation pointer */
} phoenix_tool_desc_t;

/**
 * @brief Initialize Tool Registry
 * @return 0 on success
 */
int phoenix_tool_registry_init(void);

/**
 * @brief Register a new tool into the Agent system
 * @param tool Pointer to tool descriptor
 * @return 0 on success, negative on error or registry full
 */
int phoenix_tool_register(const phoenix_tool_desc_t *tool);

/**
 * @brief Find tool descriptor by name
 * @param name Tool name
 * @return Pointer to registered descriptor or NULL if not found
 */
const phoenix_tool_desc_t* phoenix_tool_find(const char *name);

/**
 * @brief Execute a registered tool by name with arguments
 * @param name Tool name
 * @param args_json Arguments JSON string (can be NULL or empty)
 * @param result_out Buffer for output result
 * @param max_len Capacity of output buffer
 * @return 0 on success, negative on error
 */
int phoenix_tool_execute(const char *name, const char *args_json, char *result_out, size_t max_len);

struct cJSON;

/**
 * @brief Build complete Tools cJSON array object for direct injection into payload
 * @return Heap-allocated cJSON array (caller takes ownership, or adds to payload), or NULL on failure
 */
struct cJSON* phoenix_tool_build_schema_cjson(void);

/**
 * @brief Build complete Tools JSON Schema array string for LLM API request
 * @return Heap-allocated JSON string (caller must free), or NULL on failure
 */
char* phoenix_tool_build_schema_json(void);

/**
 * @brief Get count of currently registered tools
 * @return Tool count
 */
size_t phoenix_tool_get_count(void);

/**
 * @brief Get tool descriptor by index (0 to count - 1)
 * @param index Tool index
 * @return Tool descriptor pointer or NULL if index out of bounds
 */
const phoenix_tool_desc_t* phoenix_tool_get_at(size_t index);

/**
 * @brief Cleanup Tool Registry
 */
void phoenix_tool_registry_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* PHOENIX_TOOL_REGISTRY_H */
