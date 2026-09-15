/**
 * @file cartridge.h
 * @brief 场景卡带抽象接口定义 (Agent Cartridge Interface)
 * @author OpenVela Contest 2026 Team 145
 * 
 * 核心哲学: 硬件是躯壳，卡带是心智。卡带是自包含的场景业务插件，
 * 拥有独立的生命周期、UI舞台渲染、事件分发与 Web 伴侣数据流。
 */

#ifndef CARTRIDGE_H
#define CARTRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define CARTRIDGE_MAX_ID_LEN    32
#define CARTRIDGE_MAX_NAME_LEN  32
#define CARTRIDGE_MAX_ICON_LEN  16

typedef struct cartridge_s cartridge_t;

/**
 * @brief 触摸事件类型
 */
typedef enum {
    CARTRIDGE_TOUCH_DOWN = 0,
    CARTRIDGE_TOUCH_UP,
    CARTRIDGE_TOUCH_MOVE,
    CARTRIDGE_TOUCH_CLICK,
    CARTRIDGE_TOUCH_LONG_PRESS
} cartridge_touch_type_t;

/**
 * @brief 卡带操作契约表
 */
typedef struct {
    char id[CARTRIDGE_MAX_ID_LEN];        /**< 唯一标识符, 如 "familiar", "memo", "clock", "zen" */
    char name[CARTRIDGE_MAX_NAME_LEN];    /**< 中文显示名, 如 "桌面使魔", "灵感外脑" */
    char icon[CARTRIDGE_MAX_ICON_LEN];    /**< 符号或图标, 如 "🐱", "💡", "⏰", "🪷" */

    /* 生命周期回调 */
    int  (*init)(cartridge_t *self, void *user_data);
    void (*enter)(cartridge_t *self, void *stage_view); /**< 进入主屏视窗舞台 (例如 LVGL obj) */
    void (*exit)(cartridge_t *self);                    /**< 离开视窗舞台 */
    void (*destroy)(cartridge_t *self);

    /* 运行时事件驱动 */
    void (*tick_1s)(cartridge_t *self);                 /**< 1秒系统心跳 */
    void (*on_touch)(cartridge_t *self, int x, int y, cartridge_touch_type_t type); /**< 触控交互 */
    void (*on_knock)(cartridge_t *self, int intensity, int count);                 /**< 物理微震敲击 */
    void (*on_voice)(cartridge_t *self, const char *intent, const char *params_json); /**< 语音意图分发 */

    /* 伴侣 Web 交互接口 */
    int  (*get_web_status)(cartridge_t *self, char *buf, size_t max_len);          /**< 输出 JSON 状态 */
    int  (*handle_web_cmd)(cartridge_t *self, const char *cmd, const char *payload); /**< 处理 Web 指令 */
} cartridge_ops_t;

/**
 * @brief 卡带实例容器
 */
struct cartridge_s {
    cartridge_ops_t ops;
    void           *priv_data;      /**< 卡带私有上下文数据 */
    void           *current_stage;  /**< 当前挂载的 UI 舞台句柄 */
    bool            is_active;      /**< 当前是否为屏幕前台活跃卡带 */
    uint32_t        active_sec;     /**< 累计活跃使用时长(秒) */
};

#ifdef __cplusplus
}
#endif

#endif /* CARTRIDGE_H */
