/**
 * @file store.h
 * @brief Phoenix HoloDesk-S1 Persistent Data Store Engine
 * @author OpenVela Contest 2026 Team 145
 */

#ifndef PHOENIX_STORE_H
#define PHOENIX_STORE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t total_merit;          /**< Total accumulated cyber merits */
    uint32_t pomodoro_count;       /**< Completed pomodoro focus sessions */
    uint32_t total_focus_seconds;  /**< Total seconds in deep focus */
    uint32_t interaction_count;    /**< Total agent interactions */
    int64_t  last_saved_time;      /**< Unix timestamp of last flush */
} phoenix_stats_t;

/**
 * @brief Initialize persistent storage
 * @param data_dir Base directory (e.g. "/data/phoenix")
 * @return 0 on success, negative errno on failure
 */
int phoenix_store_init(const char *data_dir);

/**
 * @brief Flush and close storage
 */
void phoenix_store_deinit(void);

/**
 * @brief Load stats from JSON file into memory
 */
int phoenix_store_load(void);

/**
 * @brief Save stats from memory to JSON file immediately
 */
int phoenix_store_save(void);

/**
 * @brief Flush dirty data to disk (called periodically by UI timer)
 */
void phoenix_store_flush(void);

/**
 * @brief Mark memory data as dirty (needs save)
 */
void phoenix_store_set_dirty(void);

/**
 * @brief Get copy of current stats
 */
void phoenix_store_get_stats(phoenix_stats_t *out_stats);

/**
 * @brief Add merits and mark dirty
 */
uint32_t phoenix_store_add_merit(uint32_t delta);

/**
 * @brief Record completed pomodoro session
 */
void phoenix_store_add_pomodoro(uint32_t duration_seconds);

/**
 * @brief Record user interaction count
 */
void phoenix_store_inc_interaction(void);

#ifdef __cplusplus
}
#endif

#endif /* PHOENIX_STORE_H */
