#ifndef PHOENIX_UTILS_RING_BUFFER_H
#define PHOENIX_UTILS_RING_BUFFER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 通用定长元素环形缓冲队列
 */
typedef struct {
    uint8_t *buffer;          /**< 外部传入或内部管理的数据缓冲 */
    size_t   elem_size;       /**< 单个元素大小（字节） */
    size_t   capacity;        /**< 最大容纳元素个数 */
    size_t   head;            /**< 读取指针 */
    size_t   tail;            /**< 写入指针 */
    size_t   count;           /**< 当前元素计数 */
} ring_buffer_t;

/**
 * 初始化环形缓冲区
 * @param rb 环形缓冲句柄
 * @param buffer 外部内存区域，大小必须 >= elem_size * capacity
 * @param elem_size 单个元素大小（字节）
 * @param capacity 最大容纳元素个数
 */
void ring_buffer_init(ring_buffer_t *rb, void *buffer, size_t elem_size, size_t capacity);

/**
 * 压入一个元素（如果满了则返回 false）
 */
bool ring_buffer_push(ring_buffer_t *rb, const void *elem);

/**
 * 弹出并拷贝一个元素（如果为空则返回 false）
 */
bool ring_buffer_pop(ring_buffer_t *rb, void *elem);

/**
 * 窥视队首元素但不弹出
 */
bool ring_buffer_peek(const ring_buffer_t *rb, void *elem);

/**
 * 获取当前队列中的元素数量
 */
size_t ring_buffer_count(const ring_buffer_t *rb);

/**
 * 是否已满
 */
bool ring_buffer_is_full(const ring_buffer_t *rb);

/**
 * 是否为空
 */
bool ring_buffer_is_empty(const ring_buffer_t *rb);

/**
 * 清空队列
 */
void ring_buffer_clear(ring_buffer_t *rb);

#ifdef __cplusplus
}
#endif

#endif /* PHOENIX_UTILS_RING_BUFFER_H */
