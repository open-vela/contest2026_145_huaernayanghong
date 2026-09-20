/**
 * @file lvgl.h (Host Mock)
 * @brief Minimal LVGL Mock types and inlines for Host-side Testing
 */

#ifndef LVGL_MOCK_H
#define LVGL_MOCK_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct lv_obj_s lv_obj_t;
typedef struct lv_timer_s lv_timer_t;
typedef struct lv_font_s { int dummy; } lv_font_t;
typedef struct lv_event_s lv_event_t;

#define LV_FONT_DECLARE(font_name)
static const lv_font_t lv_font_montserrat_32 = {0};
static const lv_font_t lv_font_montserrat_30 = {0};
#define LV_SIZE_CONTENT 0
#define LV_EVENT_CLICKED 1

#define LV_PCT(x) (x)
#define LV_OPA_TRANSP 0
#define LV_OPA_COVER  255
#define LV_OPA_30     76
#define LV_OPA_70     178

#define LV_OBJ_FLAG_SCROLLABLE   (1 << 0)
#define LV_OBJ_FLAG_CLICKABLE    (1 << 1)
#define LV_OBJ_FLAG_HIDDEN       (1 << 2)
#define LV_OBJ_FLAG_EVENT_BUBBLE (1 << 3)

#define LV_PART_MAIN      0
#define LV_PART_INDICATOR 1
#define LV_PART_KNOB      2

typedef enum {
    LV_ALIGN_TOP_LEFT = 0,
    LV_ALIGN_TOP_MID,
    LV_ALIGN_TOP_RIGHT,
    LV_ALIGN_LEFT_MID,
    LV_ALIGN_CENTER,
    LV_ALIGN_RIGHT_MID,
    LV_ALIGN_BOTTOM_LEFT,
    LV_ALIGN_BOTTOM_MID,
    LV_ALIGN_BOTTOM_RIGHT
} lv_align_t;

typedef enum {
    LV_TEXT_ALIGN_LEFT = 0,
    LV_TEXT_ALIGN_CENTER,
    LV_TEXT_ALIGN_RIGHT
} lv_text_align_t;

typedef enum {
    LV_LABEL_LONG_WRAP = 0,
    LV_LABEL_LONG_DOT,
    LV_LABEL_LONG_SCROLL
} lv_label_long_mode_t;

#define LV_ANIM_OFF 0

#define LV_STATE_DEFAULT   0x0000
#define LV_STATE_CHECKED   0x0001
#define LV_STATE_FOCUSED   0x0002
#define LV_STATE_FOCUS_KEY 0x0004
#define LV_STATE_EDITED    0x0008
#define LV_STATE_HOVERED   0x0010
#define LV_STATE_PRESSED   0x0020
#define LV_STATE_SCROLLED  0x0040
#define LV_STATE_DISABLED  0x0080

typedef struct {
    uint8_t blue;
    uint8_t green;
    uint8_t red;
} lv_color_t;

static inline lv_color_t lv_color_hex(uint32_t c) {
    lv_color_t col;
    col.red = (uint8_t)((c >> 16) & 0xFF);
    col.green = (uint8_t)((c >> 8) & 0xFF);
    col.blue = (uint8_t)(c & 0xFF);
    return col;
}

static inline lv_timer_t* lv_timer_create(void (*cb)(lv_timer_t*), uint32_t period, void *user_data) {
    (void)cb; (void)period; (void)user_data;
    return (lv_timer_t*)1;
}

static inline void lv_timer_delete(lv_timer_t *timer) { (void)timer; }
static inline void lv_timer_pause(lv_timer_t *timer) { (void)timer; }
static inline void lv_timer_resume(lv_timer_t *timer) { (void)timer; }
static inline void lv_timer_reset(lv_timer_t *timer) { (void)timer; }
static inline void* lv_timer_get_user_data(const lv_timer_t *timer) { (void)timer; return NULL; }

/* Obj operations */
static inline lv_obj_t* lv_obj_create(lv_obj_t *parent) { (void)parent; return (lv_obj_t*)malloc(16); }
static inline void lv_obj_del(lv_obj_t *obj) { if (obj) free(obj); }
static inline void lv_obj_set_size(lv_obj_t *obj, int32_t w, int32_t h) { (void)obj; (void)w; (void)h; }
static inline void lv_obj_set_width(lv_obj_t *obj, int32_t w) { (void)obj; (void)w; }
static inline void lv_obj_set_height(lv_obj_t *obj, int32_t h) { (void)obj; (void)h; }
static inline void lv_obj_align(lv_obj_t *obj, lv_align_t align, int32_t x, int32_t y) { (void)obj; (void)align; (void)x; (void)y; }
static inline void lv_obj_center(lv_obj_t *obj) { (void)obj; }
static inline void lv_obj_set_pos(lv_obj_t *obj, int32_t x, int32_t y) { (void)obj; (void)x; (void)y; }

static inline void lv_obj_set_style_bg_color(lv_obj_t *obj, lv_color_t color, uint32_t selector) { (void)obj; (void)color; (void)selector; }
static inline void lv_obj_set_style_bg_opa(lv_obj_t *obj, uint8_t opa, uint32_t selector) { (void)obj; (void)opa; (void)selector; }
static inline void lv_obj_set_style_border_color(lv_obj_t *obj, lv_color_t color, uint32_t selector) { (void)obj; (void)color; (void)selector; }
static inline void lv_obj_set_style_border_width(lv_obj_t *obj, int32_t width, uint32_t selector) { (void)obj; (void)width; (void)selector; }
static inline void lv_obj_set_style_pad_all(lv_obj_t *obj, int32_t pad, uint32_t selector) { (void)obj; (void)pad; (void)selector; }
static inline void lv_obj_set_style_pad_hor(lv_obj_t *obj, int32_t pad, uint32_t selector) { (void)obj; (void)pad; (void)selector; }
static inline void lv_obj_set_style_pad_ver(lv_obj_t *obj, int32_t pad, uint32_t selector) { (void)obj; (void)pad; (void)selector; }
static inline void* lv_event_get_user_data(lv_event_t *e) { (void)e; return NULL; }
static inline void lv_obj_add_event_cb(lv_obj_t *obj, void (*cb)(lv_event_t*), uint32_t event, void *user_data) { (void)obj; (void)cb; (void)event; (void)user_data; }
static inline void lv_obj_set_style_radius(lv_obj_t *obj, int32_t radius, uint32_t selector) { (void)obj; (void)radius; (void)selector; }
static inline void lv_obj_set_style_text_color(lv_obj_t *obj, lv_color_t color, uint32_t selector) { (void)obj; (void)color; (void)selector; }
static inline void lv_obj_set_style_text_opa(lv_obj_t *obj, uint8_t opa, uint32_t selector) { (void)obj; (void)opa; (void)selector; }
static inline void lv_obj_set_style_text_align(lv_obj_t *obj, lv_text_align_t align, uint32_t selector) { (void)obj; (void)align; (void)selector; }
static inline void lv_obj_set_style_text_font(lv_obj_t *obj, const lv_font_t *font, uint32_t selector) { (void)obj; (void)font; (void)selector; }

static inline void lv_obj_clear_flag(lv_obj_t *obj, uint32_t flag) { (void)obj; (void)flag; }
static inline void lv_obj_add_flag(lv_obj_t *obj, uint32_t flag) { (void)obj; (void)flag; }
static inline void lv_obj_move_foreground(lv_obj_t *obj) { (void)obj; }
static inline void lv_obj_set_ext_click_area(lv_obj_t *obj, int32_t size) { (void)obj; (void)size; }

/* Button & Label */
static inline lv_obj_t* lv_btn_create(lv_obj_t *parent) { (void)parent; return (lv_obj_t*)malloc(16); }
static inline lv_obj_t* lv_label_create(lv_obj_t *parent) { (void)parent; return (lv_obj_t*)malloc(16); }
static inline void lv_label_set_text(lv_obj_t *obj, const char *text) { (void)obj; (void)text; }
static inline void lv_label_set_long_mode(lv_obj_t *obj, lv_label_long_mode_t mode) { (void)obj; (void)mode; }

/* Bar */
static inline lv_obj_t* lv_bar_create(lv_obj_t *parent) { (void)parent; return (lv_obj_t*)malloc(16); }
static inline void lv_bar_set_value(lv_obj_t *obj, int32_t value, int32_t anim) { (void)obj; (void)value; (void)anim; }
static inline void lv_bar_set_range(lv_obj_t *obj, int32_t min, int32_t max) { (void)obj; (void)min; (void)max; }

/* Arc */
static inline lv_obj_t* lv_arc_create(lv_obj_t *parent) { (void)parent; return (lv_obj_t*)malloc(16); }
static inline void lv_arc_set_rotation(lv_obj_t *obj, uint16_t rotation) { (void)obj; (void)rotation; }
static inline void lv_arc_set_bg_angles(lv_obj_t *obj, uint16_t start, uint16_t end) { (void)obj; (void)start; (void)end; }
static inline void lv_arc_set_range(lv_obj_t *obj, int32_t min, int32_t max) { (void)obj; (void)min; (void)max; }
static inline void lv_arc_set_value(lv_obj_t *obj, int32_t value) { (void)obj; (void)value; }
static inline void lv_obj_set_style_arc_width(lv_obj_t *obj, int32_t width, uint32_t selector) { (void)obj; (void)width; (void)selector; }
static inline void lv_obj_set_style_arc_color(lv_obj_t *obj, lv_color_t color, uint32_t selector) { (void)obj; (void)color; (void)selector; }
static inline void lv_obj_set_style_arc_rounded(lv_obj_t *obj, bool rounded, uint32_t selector) { (void)obj; (void)rounded; (void)selector; }
static inline void lv_obj_set_style_opa(lv_obj_t *obj, uint8_t opa, uint32_t selector) { (void)obj; (void)opa; (void)selector; }

/* Animation mock */
typedef struct lv_anim_s {
    void *var;
    int32_t start_val;
    int32_t end_val;
} lv_anim_t;

static inline void lv_anim_init(lv_anim_t *a) { if (a) memset(a, 0, sizeof(*a)); }
static inline void lv_anim_set_var(lv_anim_t *a, void *var) { if (a) a->var = var; }
static inline void lv_anim_set_exec_cb(lv_anim_t *a, void (*exec_cb)(void *, int32_t)) { (void)a; (void)exec_cb; }
static inline void lv_anim_set_time(lv_anim_t *a, uint32_t duration) { (void)a; (void)duration; }
static inline void lv_anim_set_values(lv_anim_t *a, int32_t start, int32_t end) { if (a) { a->start_val = start; a->end_val = end; } }
static inline void lv_anim_set_path_cb(lv_anim_t *a, void *path_cb) { (void)a; (void)path_cb; }
static inline void lv_anim_start(lv_anim_t *a) { (void)a; }
static inline void lv_anim_del(void *var, void *exec_cb) { (void)var; (void)exec_cb; }

#ifdef __cplusplus
}
#endif

#endif /* LVGL_MOCK_H */
