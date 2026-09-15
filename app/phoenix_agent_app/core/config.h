/**
 * @file config.h
 * @brief Generic Key-Value Configuration Subsystem for Phoenix HoloDesk-S1
 * @author OpenVela Contest 2026 Team 145
 */

#ifndef PHOENIX_CONFIG_H
#define PHOENIX_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define PHOENIX_CFG_VOLUME            "vol_level"
#define PHOENIX_CFG_BRIGHTNESS        "lcd_bright"
#define PHOENIX_CFG_WIFI_SSID         "wifi_ssid"
#define PHOENIX_CFG_WIFI_PSK          "wifi_psk"
#define PHOENIX_CFG_BACKEND           "llm_backend"
#define PHOENIX_CFG_API_KEY           "llm_api_key"
#define PHOENIX_CFG_MODEL             "llm_model"
#define PHOENIX_CFG_PROACTIVE_EN      "proactive_en"
#define PHOENIX_CFG_PROACTIVE_TIMEOUT "proactive_timeout_s"

/**
 * @brief Initialize configuration subsystem
 * @param storage_dir Base directory to persist config.json
 * @return 0 on success
 */
int phoenix_config_init(const char *storage_dir);

/**
 * @brief Get integer value for a configuration key
 * @param key Config key name
 * @param default_val Fallback value if key is not found
 * @return Value found or default_val
 */
int phoenix_config_get_int(const char *key, int default_val);

/**
 * @brief Set integer value for a configuration key
 * @param key Config key name
 * @param val Value to set
 * @return 0 on success
 */
int phoenix_config_set_int(const char *key, int val);

/**
 * @brief Get string value for a configuration key
 * @param key Config key name
 * @param default_val Fallback string if key not found
 * @param out_buf Destination buffer
 * @param buf_sz Size of destination buffer
 * @return Pointer to out_buf
 */
const char* phoenix_config_get_str(const char *key, const char *default_val, char *out_buf, size_t buf_sz);

/**
 * @brief Set string value for a configuration key
 * @param key Config key name
 * @param val String value to set
 * @return 0 on success
 */
int phoenix_config_set_str(const char *key, const char *val);

/**
 * @brief Flush and save configurations to persistent file
 * @return 0 on success
 */
int phoenix_config_save(void);

/**
 * @brief Close and de-initialize configuration subsystem
 */
void phoenix_config_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* PHOENIX_CONFIG_H */
