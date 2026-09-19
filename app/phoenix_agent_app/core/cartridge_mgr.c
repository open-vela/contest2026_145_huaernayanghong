/**
 * @file cartridge_mgr.c
 * @brief 场景卡带容器与调度管理器实现
 * @author OpenVela Contest 2026 Team 145
 */

#include "cartridge_mgr.h"
#include "event_bus.h"
#include "config.h"
#include "../utils/log_utils.h"
#include "../utils/time_utils.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/stat.h>

#if defined(__has_include)
#  if __has_include(<netutils/cJSON.h>)
#    include <netutils/cJSON.h>
#  elif __has_include(<cJSON/cJSON.h>)
#    include <cJSON/cJSON.h>
#  elif __has_include(<cjson/cJSON.h>)
#    include <cjson/cJSON.h>
#  elif __has_include(<cJSON.h>)
#    include <cJSON.h>
#  elif __has_include("../../../../apps/netutils/cjson/cJSON/cJSON.h")
#    include "../../../../apps/netutils/cjson/cJSON/cJSON.h"
#  else
#    include <cJSON.h>
#  endif
#else
#  include <netutils/cJSON.h>
#endif

#define TAG "CartridgeMgr"
#define CARTRIDGE_SWITCH_COOLDOWN_MS 500

static cartridge_t      s_cartridges[CARTRIDGE_MAX_REGISTRY];
static size_t           s_count = 0;
static int              s_current_index = -1;
static void            *s_stage_view = NULL;
static pthread_mutex_t  s_lock;
static pthread_once_t   s_lock_once = PTHREAD_ONCE_INIT;
static bool             s_initialized = false;
static uint64_t         s_last_switch_ms = 0;

static void init_cartridge_lock(void)
{
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&s_lock, &attr);
    pthread_mutexattr_destroy(&attr);
}

static inline void lock_mgr(void)
{
    pthread_once(&s_lock_once, init_cartridge_lock);
    pthread_mutex_lock(&s_lock);
}

static inline void unlock_mgr(void)
{
    pthread_mutex_unlock(&s_lock);
}

int cartridge_mgr_init(void *default_stage)
{
    lock_mgr();
    if (s_initialized) {
        unlock_mgr();
        return 0;
    }

    memset(s_cartridges, 0, sizeof(s_cartridges));
    s_count = 0;
    s_current_index = -1;
    s_stage_view = default_stage;
    s_initialized = true;

    unlock_mgr();
    LOG_I(TAG, "卡带管理引擎初始化成功");
    return 0;
}

void cartridge_mgr_deinit(void)
{
    lock_mgr();
    if (!s_initialized) {
        unlock_mgr();
        return;
    }

    for (size_t i = 0; i < s_count; i++) {
        if (s_cartridges[i].is_active && s_cartridges[i].ops.exit) {
            s_cartridges[i].ops.exit(&s_cartridges[i]);
            s_cartridges[i].is_active = false;
        }
        if (s_cartridges[i].ops.destroy) {
            s_cartridges[i].ops.destroy(&s_cartridges[i]);
        }
    }

    s_count = 0;
    s_current_index = -1;
    s_initialized = false;
    unlock_mgr();
    LOG_I(TAG, "卡带管理引擎已安全销毁");
}

void cartridge_mgr_set_stage(void *stage)
{
    cartridge_t *enter_cartridge = NULL;
    lock_mgr();
    s_stage_view = stage;
    if (s_current_index >= 0 && (size_t)s_current_index < s_count) {
        s_cartridges[s_current_index].current_stage = stage;
        /* 若当前卡带处于激活状态且新舞台有效，记录待装载卡带 */
        if (stage && s_cartridges[s_current_index].is_active && s_cartridges[s_current_index].ops.enter) {
            enter_cartridge = &s_cartridges[s_current_index];
        }
    }
    unlock_mgr();

    /* 架构级防死锁：在互斥锁外执行业务层舞台装载回调 */
    if (enter_cartridge && enter_cartridge->ops.enter) {
        enter_cartridge->ops.enter(enter_cartridge, stage);
    }
}

int cartridge_mgr_register(const cartridge_ops_t *ops, void *priv_data, void *user_init_data)
{
    if (!ops || !ops->id[0]) {
        return -1;
    }

    lock_mgr();
    if (!s_initialized) {
        unlock_mgr();
        return -2;
    }

    if (s_count >= CARTRIDGE_MAX_REGISTRY) {
        unlock_mgr();
        LOG_W(TAG, "注册失败: 卡带槽位已满 (上限 %d)", CARTRIDGE_MAX_REGISTRY);
        return -3;
    }

    /* 查重 */
    for (size_t i = 0; i < s_count; i++) {
        if (strncmp(s_cartridges[i].ops.id, ops->id, CARTRIDGE_MAX_ID_LEN) == 0) {
            unlock_mgr();
            LOG_W(TAG, "注册失败: 卡带 ID 重复: %s", ops->id);
            return -4;
        }
    }

    size_t idx = s_count;
    memset(&s_cartridges[idx], 0, sizeof(cartridge_t));
    memcpy(&s_cartridges[idx].ops, ops, sizeof(cartridge_ops_t));
    s_cartridges[idx].priv_data = priv_data;
    s_cartridges[idx].is_active = false;
    s_cartridges[idx].current_stage = NULL;

    if (s_cartridges[idx].ops.init) {
        int ret = s_cartridges[idx].ops.init(&s_cartridges[idx], user_init_data);
        if (ret != 0) {
            LOG_W(TAG, "卡带 [%s] 初始化失败, 错误码: %d", ops->id, ret);
            unlock_mgr();
            return -5;
        }
    }

    s_count++;
    LOG_I(TAG, "成功注册卡带 [%s - %s %s], 当前总数: %zu", 
         ops->id, ops->name, ops->icon, s_count);

    /* 若是第一个注册的卡带，自动激活进入 */
    if (s_count == 1) {
        s_current_index = 0;
        s_cartridges[0].is_active = true;
        s_cartridges[0].current_stage = s_stage_view;
        if (s_cartridges[0].ops.enter) {
            s_cartridges[0].ops.enter(&s_cartridges[0], s_stage_view);
        }
    }

    unlock_mgr();
    return 0;
}

int cartridge_mgr_unregister(const char *id)
{
    if (!id || !id[0]) return -1;

    lock_mgr();
    int target_idx = -1;
    for (size_t i = 0; i < s_count; i++) {
        if (strncmp(s_cartridges[i].ops.id, id, CARTRIDGE_MAX_ID_LEN) == 0) {
            target_idx = (int)i;
            break;
        }
    }

    if (target_idx < 0) {
        unlock_mgr();
        return -2;
    }

    /* 若注销的是当前活跃卡带，先退出 */
    if (target_idx == s_current_index) {
        if (s_cartridges[target_idx].ops.exit) {
            s_cartridges[target_idx].ops.exit(&s_cartridges[target_idx]);
        }
        s_cartridges[target_idx].is_active = false;
    }

    if (s_cartridges[target_idx].ops.destroy) {
        s_cartridges[target_idx].ops.destroy(&s_cartridges[target_idx]);
    }

    /* 移动填补槽位 */
    for (size_t i = target_idx; i + 1 < s_count; i++) {
        s_cartridges[i] = s_cartridges[i + 1];
    }
    s_count--;

    if (s_count == 0) {
        s_current_index = -1;
    } else if (s_current_index >= (int)s_count) {
        s_current_index = 0;
        s_cartridges[0].is_active = true;
        s_cartridges[0].current_stage = s_stage_view;
        if (s_cartridges[0].ops.enter) {
            s_cartridges[0].ops.enter(&s_cartridges[0], s_stage_view);
        }
    }

    unlock_mgr();
    LOG_I(TAG, "已注销卡带: %s, 剩余卡带数: %zu", id, s_count);
    return 0;
}

typedef struct {
    bool has_events;
    phoenix_event_data_t switch_evt;
    phoenix_event_data_t sound_evt;
} switch_event_bundle_t;

static int do_switch_locked(int new_idx, switch_event_bundle_t *bundle)
{
    if (new_idx < 0 || (size_t)new_idx >= s_count) {
        return -1;
    }

    if (new_idx == s_current_index && s_cartridges[new_idx].is_active) {
        return 0; /* 已经在当前卡带 */
    }

    const char *from_id = "none";
    if (s_current_index >= 0 && (size_t)s_current_index < s_count) {
        from_id = s_cartridges[s_current_index].ops.id;
        if (s_cartridges[s_current_index].is_active && s_cartridges[s_current_index].ops.exit) {
            s_cartridges[s_current_index].ops.exit(&s_cartridges[s_current_index]);
        }
        s_cartridges[s_current_index].is_active = false;
    }

    s_current_index = new_idx;
    s_cartridges[new_idx].is_active = true;
    s_cartridges[new_idx].current_stage = s_stage_view;
    if (s_cartridges[new_idx].ops.enter) {
        s_cartridges[new_idx].ops.enter(&s_cartridges[new_idx], s_stage_view);
    }

    s_last_switch_ms = time_utils_get_ms();

    LOG_I(TAG, "🔄 卡带切换: [%s] -> [%s - %s %s]",
         from_id, s_cartridges[new_idx].ops.id, 
         s_cartridges[new_idx].ops.name, s_cartridges[new_idx].ops.icon);

    /* 持久化配置 */
    phoenix_config_set_str("active_cartridge", s_cartridges[new_idx].ops.id);

    /*
     * 架构级防死锁（方案 A）：
     * 严禁在持有内部锁时调用外部广播或派发事件。
     * 将需发布的事件暂存至栈上 bundle，由外层调用者在释放互斥锁后再统一派发。
     */
    if (bundle) {
        bundle->has_events = true;

        memset(&bundle->switch_evt, 0, sizeof(bundle->switch_evt));
        bundle->switch_evt.type = PHOENIX_EVT_CARTRIDGE_SWITCHED;
        bundle->switch_evt.data.cartridge.from_id = from_id;
        bundle->switch_evt.data.cartridge.to_id = s_cartridges[new_idx].ops.id;
        bundle->switch_evt.data.cartridge.name = s_cartridges[new_idx].ops.name;
        bundle->switch_evt.data.cartridge.icon = s_cartridges[new_idx].ops.icon;
        bundle->switch_evt.data.cartridge.index = (size_t)new_idx;
        bundle->switch_evt.data.cartridge.total = s_count;

        memset(&bundle->sound_evt, 0, sizeof(bundle->sound_evt));
        bundle->sound_evt.type = PHOENIX_EVT_PLAY_SOUND;
        bundle->sound_evt.data.sound.sound_id = 1; /* 提示微音 */
    }

    return 0;
}

static void publish_switch_events(const switch_event_bundle_t *bundle)
{
    if (!bundle || !bundle->has_events) {
        return;
    }
    phoenix_event_publish(&bundle->switch_evt);
    phoenix_event_publish(&bundle->sound_evt);
}

int cartridge_mgr_switch_to(const char *id)
{
    if (!id || !id[0]) return -1;

    switch_event_bundle_t bundle = {0};
    lock_mgr();
    int target_idx = -1;
    for (size_t i = 0; i < s_count; i++) {
        if (strncmp(s_cartridges[i].ops.id, id, CARTRIDGE_MAX_ID_LEN) == 0) {
            target_idx = (int)i;
            break;
        }
    }

    if (target_idx < 0) {
        unlock_mgr();
        LOG_W(TAG, "未找到目标卡带: %s", id);
        return -2;
    }

    int ret = do_switch_locked(target_idx, &bundle);
    unlock_mgr();

    if (ret == 0) {
        publish_switch_events(&bundle);
    }
    return ret;
}

int cartridge_mgr_next(void)
{
    switch_event_bundle_t bundle = {0};
    lock_mgr();
    if (s_count <= 1) {
        unlock_mgr();
        return 0;
    }

    int next_idx = (s_current_index + 1) % (int)s_count;
    int ret = do_switch_locked(next_idx, &bundle);
    unlock_mgr();

    if (ret == 0) {
        publish_switch_events(&bundle);
    }
    return ret;
}

int cartridge_mgr_prev(void)
{
    switch_event_bundle_t bundle = {0};
    lock_mgr();
    if (s_count <= 1) {
        unlock_mgr();
        return 0;
    }

    int prev_idx = (s_current_index - 1 + (int)s_count) % (int)s_count;
    int ret = do_switch_locked(prev_idx, &bundle);
    unlock_mgr();

    if (ret == 0) {
        publish_switch_events(&bundle);
    }
    return ret;
}

cartridge_t *cartridge_mgr_get_current(void)
{
    lock_mgr();
    if (s_current_index >= 0 && (size_t)s_current_index < s_count) {
        cartridge_t *res = &s_cartridges[s_current_index];
        unlock_mgr();
        return res;
    }
    unlock_mgr();
    return NULL;
}

size_t cartridge_mgr_get_count(void)
{
    lock_mgr();
    size_t count = s_count;
    unlock_mgr();
    return count;
}

cartridge_t *cartridge_mgr_get_by_index(size_t index)
{
    lock_mgr();
    if (index < s_count) {
        cartridge_t *res = &s_cartridges[index];
        unlock_mgr();
        return res;
    }
    unlock_mgr();
    return NULL;
}

cartridge_t *cartridge_mgr_get_by_id(const char *id)
{
    if (!id) return NULL;
    lock_mgr();
    for (size_t i = 0; i < s_count; i++) {
        if (strncmp(s_cartridges[i].ops.id, id, CARTRIDGE_MAX_ID_LEN) == 0) {
            cartridge_t *res = &s_cartridges[i];
            unlock_mgr();
            return res;
        }
    }
    unlock_mgr();
    return NULL;
}

void cartridge_mgr_dispatch_tick_1s(void)
{
    lock_mgr();
    if (s_current_index >= 0 && (size_t)s_current_index < s_count) {
        cartridge_t *c = &s_cartridges[s_current_index];
        c->active_sec++;
        if (c->ops.tick_1s) {
            c->ops.tick_1s(c);
        }
    }
    unlock_mgr();
}

void cartridge_mgr_dispatch_touch(int x, int y, cartridge_touch_type_t type)
{
    lock_mgr();
    if (s_current_index >= 0 && (size_t)s_current_index < s_count) {
        cartridge_t *c = &s_cartridges[s_current_index];
        if (c->ops.on_touch) {
            c->ops.on_touch(c, x, y, type);
        }
    }
    unlock_mgr();
}

void cartridge_mgr_dispatch_knock(int intensity, int count)
{
    /* 规则：连续快速三连敲(count == 3)，系统级免手操顺时针轮换到下一个卡带 */
    if (count == 3) {
        uint64_t now_ms = time_utils_get_ms();
        if (now_ms - s_last_switch_ms < CARTRIDGE_SWITCH_COOLDOWN_MS) {
            LOG_D(TAG, "三连敲在冷却期内 (%llu ms)，忽略防抖", (unsigned long long)(now_ms - s_last_switch_ms));
            return;
        }
        LOG_I(TAG, "检测到桌面连续快速三连敲，触发免手操卡带轮换！");
        cartridge_mgr_next();
        return;
    }

    lock_mgr();
    if (s_current_index >= 0 && (size_t)s_current_index < s_count) {
        cartridge_t *c = &s_cartridges[s_current_index];
        if (c->ops.on_knock) {
            c->ops.on_knock(c, intensity, count);
        }
    }
    unlock_mgr();
}

void cartridge_mgr_dispatch_voice(const char *intent, const char *params_json)
{
    lock_mgr();
    if (s_current_index >= 0 && (size_t)s_current_index < s_count) {
        cartridge_t *c = &s_cartridges[s_current_index];
        if (c->ops.on_voice) {
            c->ops.on_voice(c, intent, params_json);
        }
    }
    unlock_mgr();
}

static int dynamic_cartridge_init(cartridge_t *self, void *user_data)
{
    (void)user_data;
    LOG_I(TAG, "动态扩展卡带 [%s] 初始化成功", self ? self->ops.name : "");
    return 0;
}

static void dynamic_cartridge_enter(cartridge_t *self, void *stage_view)
{
    (void)stage_view;
    if (!self) return;
    LOG_I(TAG, "进入动态扩展卡带视窗: [%s]", self->ops.name);
    phoenix_event_data_t fly_evt;
    memset(&fly_evt, 0, sizeof(fly_evt));
    fly_evt.type = PHOENIX_EVT_FLYING_TEXT;
    fly_evt.data.flying_text.text = self->ops.name;
    fly_evt.data.flying_text.color_rgb = 0x00e5ff;
    phoenix_event_publish(&fly_evt);
}

static void dynamic_cartridge_exit(cartridge_t *self)
{
    if (!self) return;
    LOG_I(TAG, "离开动态扩展卡带视窗: [%s]", self->ops.name);
}

int cartridge_mgr_scan_external(const char *cartridges_root)
{
    if (!cartridges_root || !cartridges_root[0]) {
        cartridges_root = "/sdcard/cartridges";
    }

    DIR *dir = opendir(cartridges_root);
    if (!dir) {
        LOG_D(TAG, "未发现外部卡带目录或未插入 SD 卡: %s", cartridges_root);
        return 0;
    }

    int loaded_count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;

        char manifest_path[512];
        snprintf(manifest_path, sizeof(manifest_path), "%s/%s/manifest.json", cartridges_root, entry->d_name);

        FILE *f = fopen(manifest_path, "rb");
        if (!f) {
            snprintf(manifest_path, sizeof(manifest_path), "%s/%s", cartridges_root, entry->d_name);
            if (strstr(entry->d_name, ".json") == NULL) {
                continue;
            }
            f = fopen(manifest_path, "rb");
            if (!f) continue;
        }

        fseek(f, 0, SEEK_END);
        long len = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (len <= 0 || len > 16384) {
            fclose(f);
            continue;
        }

        char *content = (char *)malloc(len + 1);
        if (!content) {
            fclose(f);
            continue;
        }
        size_t read_bytes = fread(content, 1, len, f);
        content[read_bytes] = '\0';
        fclose(f);

        cJSON *root = cJSON_Parse(content);
        free(content);
        if (!root) continue;

        cJSON *id_item = cJSON_GetObjectItem(root, "id");
        cJSON *name_item = cJSON_GetObjectItem(root, "name");
        cJSON *icon_item = cJSON_GetObjectItem(root, "icon");

        if (id_item && id_item->valuestring && name_item && name_item->valuestring) {
            cartridge_ops_t ops;
            memset(&ops, 0, sizeof(ops));
            strncpy(ops.id, id_item->valuestring, sizeof(ops.id) - 1);
            strncpy(ops.name, name_item->valuestring, sizeof(ops.name) - 1);
            strncpy(ops.icon, icon_item && icon_item->valuestring ? icon_item->valuestring : "📦", sizeof(ops.icon) - 1);
            ops.init = dynamic_cartridge_init;
            ops.enter = dynamic_cartridge_enter;
            ops.exit = dynamic_cartridge_exit;

            if (cartridge_mgr_register(&ops, NULL, NULL) == 0) {
                loaded_count++;
                LOG_I(TAG, "✨ 成功热加载外部卡带: [%s - %s %s]", ops.id, ops.icon, ops.name);
            }
        }
        cJSON_Delete(root);
    }
    closedir(dir);
    return loaded_count;
}
