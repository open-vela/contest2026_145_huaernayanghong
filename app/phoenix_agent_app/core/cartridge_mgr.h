/**
 * @file cartridge_mgr.h
 * @brief 场景卡带容器与调度管理器 (Cartridge Manager Engine)
 * @author OpenVela Contest 2026 Team 145
 */

#ifndef CARTRIDGE_MGR_H
#define CARTRIDGE_MGR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "cartridge.h"

#define CARTRIDGE_MAX_REGISTRY  8

/**
 * @brief 初始化卡带管理引擎
 * @param default_stage 默认挂载的 UI 舞台指针 (如 LVGL 容器句柄)
 * @return 0 成功, 负数失败
 */
int cartridge_mgr_init(void *default_stage);

/**
 * @brief 销毁卡带管理引擎，释放所有注册卡带
 */
void cartridge_mgr_deinit(void);

/**
 * @brief 设置或更新主屏视窗舞台
 * @param stage UI 舞台指针
 */
void cartridge_mgr_set_stage(void *stage);

/**
 * @brief 注册新卡带
 * @param ops 卡带操作契约
 * @param priv_data 卡带私有数据
 * @param user_init_data 传递给 init 回调的初始化参数
 * @return 0 成功, 负数失败
 */
int cartridge_mgr_register(const cartridge_ops_t *ops, void *priv_data, void *user_init_data);

/**
 * @brief 注销指定卡带
 * @param id 卡带唯一标识
 * @return 0 成功, 负数失败
 */
int cartridge_mgr_unregister(const char *id);

/**
 * @brief 切换到指定 ID 的卡带
 * @param id 卡带 ID
 * @return 0 成功, 负数失败
 */
int cartridge_mgr_switch_to(const char *id);

/**
 * @brief 顺时针轮换到下一个卡带
 * @return 0 成功, 负数失败
 */
int cartridge_mgr_next(void);

/**
 * @brief 逆时针轮换到上一个卡带
 * @return 0 成功, 负数失败
 */
int cartridge_mgr_prev(void);

/**
 * @brief 获取当前正在运行的卡带
 * @return 卡带指针, 若无则返回 NULL
 */
cartridge_t *cartridge_mgr_get_current(void);

/**
 * @brief 获取已注册卡带数量
 * @return 卡带数量
 */
size_t cartridge_mgr_get_count(void);

/**
 * @brief 根据索引获取卡带
 * @param index 索引 (0 ~ count-1)
 * @return 卡带指针
 */
cartridge_t *cartridge_mgr_get_by_index(size_t index);

/**
 * @brief 根据 ID 获取卡带
 * @param id 卡带 ID
 * @return 卡带指针
 */
cartridge_t *cartridge_mgr_get_by_id(const char *id);

/* --- 系统事件分发接口 --- */

/**
 * @brief 分发 1 秒系统心跳
 */
void cartridge_mgr_dispatch_tick_1s(void);

/**
 * @brief 分发触控手势事件
 * @param x 坐标 X
 * @param y 坐标 Y
 * @param type 触摸类型
 */
void cartridge_mgr_dispatch_touch(int x, int y, cartridge_touch_type_t type);

/**
 * @brief 分发物理敲击事件
 * @param intensity 敲击震动强度 (1~100)
 * @param count 连续敲击次数 (1=单敲, 2=双敲, 3=三连敲)
 */
void cartridge_mgr_dispatch_knock(int intensity, int count);

/**
 * @brief 分发语音意图
 * @param intent 意图名称
 * @param params_json 参数 JSON
 */
void cartridge_mgr_dispatch_voice(const char *intent, const char *params_json);

#ifdef __cplusplus
}
#endif

#endif /* CARTRIDGE_MGR_H */
