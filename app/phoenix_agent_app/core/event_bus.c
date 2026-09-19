/**
 * @file event_bus.c
 * @brief Lightweight Publish-Subscribe Event Bus Implementation
 * @author OpenVela Contest 2026 Team 145
 */

#include "event_bus.h"
#include "../utils/ring_buffer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define MAX_SUBSCRIBERS_PER_EVENT 8
#define EVENT_QUEUE_CAPACITY      64

typedef struct {
    phoenix_event_cb_t cb;
    void *user_data;
} subscriber_entry_t;

typedef struct {
    subscriber_entry_t entries[MAX_SUBSCRIBERS_PER_EVENT];
    size_t count;
} event_slot_t;

static event_slot_t g_event_slots[PHOENIX_EVT_COUNT];
static phoenix_event_data_t g_queue_storage[EVENT_QUEUE_CAPACITY];
static ring_buffer_t g_async_ring_buffer;
static pthread_mutex_t g_queue_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_bus_lock = PTHREAD_MUTEX_INITIALIZER;
static bool g_bus_initialized = false;
static pthread_t g_main_thread = 0;
static bool g_main_thread_set = false;

void phoenix_event_bus_set_main_thread(pthread_t tid)
{
    g_main_thread = tid;
    g_main_thread_set = true;
}

bool phoenix_event_bus_is_main_thread(void)
{
    if (!g_main_thread_set) return true;
    return (pthread_equal(pthread_self(), g_main_thread) != 0);
}

int phoenix_event_bus_init(void)
{
    pthread_mutex_lock(&g_bus_lock);
    memset(g_event_slots, 0, sizeof(g_event_slots));
    pthread_mutex_unlock(&g_bus_lock);

    pthread_mutex_lock(&g_queue_lock);
    ring_buffer_init(&g_async_ring_buffer, g_queue_storage, sizeof(phoenix_event_data_t), EVENT_QUEUE_CAPACITY);
    pthread_mutex_unlock(&g_queue_lock);

    g_main_thread = pthread_self();
    g_main_thread_set = true;
    g_bus_initialized = true;
    return 0;
}

int phoenix_event_subscribe(phoenix_event_type_t type, phoenix_event_cb_t cb, void *user_data)
{
    if (!g_bus_initialized || type <= PHOENIX_EVT_NONE || type >= PHOENIX_EVT_COUNT || !cb) {
        return -1;
    }

    pthread_mutex_lock(&g_bus_lock);
    event_slot_t *slot = &g_event_slots[type];
    if (slot->count >= MAX_SUBSCRIBERS_PER_EVENT) {
        pthread_mutex_unlock(&g_bus_lock);
        return -2; /* Capacity full */
    }

    /* Check duplicate */
    for (size_t i = 0; i < slot->count; i++) {
        if (slot->entries[i].cb == cb && slot->entries[i].user_data == user_data) {
            pthread_mutex_unlock(&g_bus_lock);
            return 0; /* Already subscribed */
        }
    }

    slot->entries[slot->count].cb = cb;
    slot->entries[slot->count].user_data = user_data;
    slot->count++;
    pthread_mutex_unlock(&g_bus_lock);
    return 0;
}

void phoenix_event_unsubscribe(phoenix_event_type_t type, phoenix_event_cb_t cb, void *user_data)
{
    if (!g_bus_initialized || type <= PHOENIX_EVT_NONE || type >= PHOENIX_EVT_COUNT || !cb) {
        return;
    }

    pthread_mutex_lock(&g_bus_lock);
    event_slot_t *slot = &g_event_slots[type];
    for (size_t i = 0; i < slot->count; i++) {
        if (slot->entries[i].cb == cb && slot->entries[i].user_data == user_data) {
            /* Shift down */
            for (size_t j = i; j < slot->count - 1; j++) {
                slot->entries[j] = slot->entries[j + 1];
            }
            slot->count--;
            break;
        }
    }
    pthread_mutex_unlock(&g_bus_lock);
}

void phoenix_event_publish(const phoenix_event_data_t *event)
{
    if (!g_bus_initialized || !event || event->type <= PHOENIX_EVT_NONE || event->type >= PHOENIX_EVT_COUNT) {
        return;
    }

    /*
     * 跨线程并发安全防护：
     * 若调用者处于非 UI 主线程（如网络看门狗、配网 Worker、后台大模型推理等线程），
     * 自动将事件路由至异步环形队列，交由主线程 phoenix_app_tick 串行消费分发，
     * 彻底杜绝后台线程并发执行订阅者的 UI/LVGL 操作引发链表死锁与内存崩溃。
     */
    if (g_main_thread_set && !pthread_equal(pthread_self(), g_main_thread)) {
        phoenix_event_post_async(event);
        return;
    }

    /*
     * 快照派发机制：在持有 g_bus_lock 时将订阅者列表拷贝至线程局部栈中，
     * 随后释放锁再依次执行回调，避免回调内部递归操作 EventBus 发生自死锁。
     */
    subscriber_entry_t snapshot[MAX_SUBSCRIBERS_PER_EVENT];
    size_t count = 0;

    pthread_mutex_lock(&g_bus_lock);
    event_slot_t *slot = &g_event_slots[event->type];
    count = slot->count;
    if (count > MAX_SUBSCRIBERS_PER_EVENT) count = MAX_SUBSCRIBERS_PER_EVENT;
    for (size_t i = 0; i < count; i++) {
        snapshot[i] = slot->entries[i];
    }
    pthread_mutex_unlock(&g_bus_lock);

    for (size_t i = 0; i < count; i++) {
        if (snapshot[i].cb) {
            snapshot[i].cb(event, snapshot[i].user_data);
        }
    }
}

int phoenix_event_post_async(const phoenix_event_data_t *event)
{
    if (!g_bus_initialized || !event || event->type <= PHOENIX_EVT_NONE || event->type >= PHOENIX_EVT_COUNT) {
        return -1;
    }

    pthread_mutex_lock(&g_queue_lock);
    bool pushed = ring_buffer_push(&g_async_ring_buffer, event);
    pthread_mutex_unlock(&g_queue_lock);

    return pushed ? 0 : -2; /* -2: Queue overflow */
}

size_t phoenix_event_bus_drain(void)
{
    if (!g_bus_initialized) {
        return 0;
    }

    /* 跨线程并发防护：事件排空仅允许 UI 主线程串行分发，禁止工作/网络线程调用避免事件重推异步环形队列引发活锁 */
    if (g_main_thread_set && !pthread_equal(pthread_self(), g_main_thread)) {
        return 0;
    }

    size_t dispatched = 0;
    while (1) {
        phoenix_event_data_t evt;
        pthread_mutex_lock(&g_queue_lock);
        bool popped = ring_buffer_pop(&g_async_ring_buffer, &evt);
        pthread_mutex_unlock(&g_queue_lock);

        if (!popped) {
            break;
        }

        phoenix_event_publish(&evt);
        dispatched++;
    }

    return dispatched;
}

size_t phoenix_event_bus_pending_count(void)
{
    if (!g_bus_initialized) {
        return 0;
    }
    pthread_mutex_lock(&g_queue_lock);
    size_t count = ring_buffer_count(&g_async_ring_buffer);
    pthread_mutex_unlock(&g_queue_lock);
    return count;
}

void phoenix_event_bus_deinit(void)
{
    pthread_mutex_lock(&g_queue_lock);
    ring_buffer_clear(&g_async_ring_buffer);
    pthread_mutex_unlock(&g_queue_lock);

    pthread_mutex_lock(&g_bus_lock);
    memset(g_event_slots, 0, sizeof(g_event_slots));
    g_main_thread_set = false;
    g_bus_initialized = false;
    pthread_mutex_unlock(&g_bus_lock);
}
