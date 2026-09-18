/**
 * @file event_bus.h
 * @brief Lightweight Publish-Subscribe Event Bus for Phoenix HoloDesk-S1
 * @author OpenVela Contest 2026 Team 145
 */

#ifndef PHOENIX_EVENT_BUS_H
#define PHOENIX_EVENT_BUS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <pthread.h>

/**
 * @brief Event Types in Phoenix Agent System
 */
typedef enum {
    PHOENIX_EVT_NONE = 0,
    PHOENIX_EVT_STATE_CHANGED,    /**< Agent state changed (e.g. IDLE, THINKING) */
    PHOENIX_EVT_FLYING_TEXT,      /**< Flying text animation on UI */
    PHOENIX_EVT_PLAY_SOUND,       /**< Play audio effect */
    PHOENIX_EVT_TOOL_TRIGGERED,   /**< Tool called / executed */
    PHOENIX_EVT_LLM_CHUNK,        /**< LLM streaming text token */
    PHOENIX_EVT_LLM_THINKING,     /**< LLM reasoning / thinking process snippet */
    PHOENIX_EVT_LLM_FINISHED,     /**< LLM complete response */
    PHOENIX_EVT_POMODORO_TICK,    /**< Pomodoro countdown 1s tick */
    PHOENIX_EVT_MERIT_UPDATED,    /**< Cyber merit / PR count updated */
    PHOENIX_EVT_CI_ALERT,         /**< CI/CD status notification */
    PHOENIX_EVT_PROACTIVE_INTERVENE, /**< Proactive Agent intervention (Threshold, Schedule, Context) */
    PHOENIX_EVT_HAL_TAP,          /**< Hardware physical tap / knock detected */
    PHOENIX_EVT_HAL_LIGHT,        /**< Ambient light sensor change (Lux) */
    PHOENIX_EVT_HAL_BATTERY,      /**< Power & battery level update */
    PHOENIX_EVT_CARTRIDGE_SWITCHED, /**< Active cartridge switched */
    PHOENIX_EVT_NET_STATUS,       /**< Wi-Fi network mode, SSID, IP and state change */
    PHOENIX_EVT_COUNT
} phoenix_event_type_t;

/**
 * @brief Proactive intervention category
 */
typedef enum {
    PROACTIVE_THRESHOLD_FATIGUE = 0, /**< Focus timeout / health break */
    PROACTIVE_PERIODIC_BRIEFING,     /**< Morning / scheduled daily briefing */
    PROACTIVE_CONTEXT_MILESTONE,     /**< Merit milestone celebration */
    PROACTIVE_SYSTEM_HEALTH,         /**< System health self-healing alert */
    PROACTIVE_ENV_DARK_SLEEP,        /**< Low ambient light night-mode / white noise */
    PROACTIVE_BATTERY_LOW            /**< Critical low battery power-saving guard */
} phoenix_proactive_type_t;

/**
 * @brief Event payload union
 */
typedef struct {
    phoenix_event_type_t type;
    union {
        struct {
            int old_state;
            int new_state;
            const char *message;
        } state;

        struct {
            const char *text;
            uint32_t color_rgb;   /**< 24-bit RGB color (e.g. 0x00E5FF, 0xFFD700) */
        } flying_text;

        struct {
            int sound_id;
        } sound;

        struct {
            const char *tool_name;
            const char *result_summary;
            bool success;
        } tool;

        struct {
            const char *reasoning_snippet;
        } thinking;

        struct {
            int proactive_type;   /**< phoenix_proactive_type_t */
            const char *title;
            const char *suggestion;
            const char *auto_action;
        } proactive;

        struct {
            const char *text;
            bool is_final;
        } llm_text;

        struct {
            uint16_t remaining_s;
            bool is_active;
            uint32_t total_merit;
            uint32_t uptime_s;
        } stats;

        struct {
            bool passed;
            const char *pipeline_name;
            const char *detail;
        } ci;

        struct {
            int intensity;
            uint64_t timestamp_ms;
        } tap;

        struct {
            uint32_t lux;
            bool is_dark_mode;
        } light;

        struct {
            uint8_t percentage;
            bool is_charging;
            bool is_low_power;
        } battery;

        struct {
            const char *from_id;
            const char *to_id;
            const char *name;
            const char *icon;
            size_t index;
            size_t total;
        } cartridge;

        struct {
            int mode;              /**< net_mode_t */
            const char *ssid;
            const char *ip;
            const char *msg;
        } net;
    } data;
} phoenix_event_data_t;

/**
 * @brief Callback prototype for event subscribers
 */
typedef void (*phoenix_event_cb_t)(const phoenix_event_data_t *event, void *user_data);

/**
 * @brief Initialize Event Bus
 * @return 0 on success, negative on failure
 */
int phoenix_event_bus_init(void);

/**
 * @brief Subscribe to a specific event type
 * @param type Event type
 * @param cb Callback function
 * @param user_data User context pointer passed to callback
 * @return 0 on success, negative on error or full capacity
 */
int phoenix_event_subscribe(phoenix_event_type_t type, phoenix_event_cb_t cb, void *user_data);

/**
 * @brief Unsubscribe callback from an event type
 * @param type Event type
 * @param cb Callback function to remove
 * @param user_data Matching user context pointer
 */
void phoenix_event_unsubscribe(phoenix_event_type_t type, phoenix_event_cb_t cb, void *user_data);

/**
 * @brief Publish an event synchronously to all registered subscribers
 * @param event Pointer to event data structure
 */
void phoenix_event_publish(const phoenix_event_data_t *event);

/**
 * @brief Enqueue an event asynchronously (thread-safe, non-blocking)
 * @param event Pointer to event data structure
 * @return 0 on success, negative if queue is full or uninitialized
 */
int phoenix_event_post_async(const phoenix_event_data_t *event);

/**
 * @brief Drain and dispatch all queued events to subscribers in caller's thread
 * @return Number of events dispatched
 */
size_t phoenix_event_bus_drain(void);

/**
 * @brief Get number of pending events in async queue
 * @return Count of pending events
 */
size_t phoenix_event_bus_pending_count(void);

/**
 * @brief Set the designated UI main thread ID for cross-thread dispatch safety
 * @param tid pthread ID of the main thread
 */
void phoenix_event_bus_set_main_thread(pthread_t tid);

/**
 * @brief Check whether caller is running on the registered main thread
 * @return true if running on main thread or unconstrained
 */
bool phoenix_event_bus_is_main_thread(void);

/**
 * @brief De-initialize and cleanup Event Bus
 */
void phoenix_event_bus_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* PHOENIX_EVENT_BUS_H */
