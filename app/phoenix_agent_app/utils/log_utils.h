#ifndef PHOENIX_UTILS_LOG_UTILS_H
#define PHOENIX_UTILS_LOG_UTILS_H

#include <stdio.h>

#define PHOENIX_LOG_COLOR_RESET   "\033[0m"
#define PHOENIX_LOG_COLOR_RED     "\033[31m"
#define PHOENIX_LOG_COLOR_GREEN   "\033[32m"
#define PHOENIX_LOG_COLOR_YELLOW  "\033[33m"
#define PHOENIX_LOG_COLOR_BLUE    "\033[34m"
#define PHOENIX_LOG_COLOR_CYAN    "\033[36m"
#define PHOENIX_LOG_COLOR_PURPLE  "\033[35m"

#ifndef PHOENIX_LOG_LEVEL
#define PHOENIX_LOG_LEVEL 3 /* 0: None, 1: Error, 2: Warn, 3: Info, 4: Debug */
#endif

#if (PHOENIX_LOG_LEVEL >= 4)
#define LOG_D(tag, fmt, ...) printf(PHOENIX_LOG_COLOR_CYAN "[%s:D] " fmt PHOENIX_LOG_COLOR_RESET "\n", tag, ##__VA_ARGS__)
#else
#define LOG_D(tag, fmt, ...) ((void)0)
#endif

#if (PHOENIX_LOG_LEVEL >= 3)
#define LOG_I(tag, fmt, ...) printf(PHOENIX_LOG_COLOR_GREEN "[%s:I] " fmt PHOENIX_LOG_COLOR_RESET "\n", tag, ##__VA_ARGS__)
#else
#define LOG_I(tag, fmt, ...) ((void)0)
#endif

#if (PHOENIX_LOG_LEVEL >= 2)
#define LOG_W(tag, fmt, ...) printf(PHOENIX_LOG_COLOR_YELLOW "[%s:W] " fmt PHOENIX_LOG_COLOR_RESET "\n", tag, ##__VA_ARGS__)
#else
#define LOG_W(tag, fmt, ...) ((void)0)
#endif

#if (PHOENIX_LOG_LEVEL >= 1)
#define LOG_E(tag, fmt, ...) printf(PHOENIX_LOG_COLOR_RED "[%s:E] " fmt PHOENIX_LOG_COLOR_RESET "\n", tag, ##__VA_ARGS__)
#else
#define LOG_E(tag, fmt, ...) ((void)0)
#endif

#endif /* PHOENIX_UTILS_LOG_UTILS_H */
