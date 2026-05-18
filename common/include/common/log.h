#ifndef SOTARK_COMMON_LOG_H
#define SOTARK_COMMON_LOG_H

#include <stdio.h>
#include <stdlib.h>

#define LOG_INFO(...)  do { fprintf(stdout, "[info]  " __VA_ARGS__); fputc('\n', stdout); } while (0)
#define LOG_WARN(...)  do { fprintf(stderr, "[warn]  " __VA_ARGS__); fputc('\n', stderr); } while (0)
#define LOG_ERROR(...) do { fprintf(stderr, "[error] " __VA_ARGS__); fputc('\n', stderr); } while (0)

#define PANIC(...) do {                                          \
    fprintf(stderr, "[PANIC] %s:%d: ", __FILE__, __LINE__);      \
    fprintf(stderr, __VA_ARGS__);                                \
    fputc('\n', stderr);                                         \
    abort();                                                     \
} while (0)

#define ASSERT(cond) do { if (!(cond)) PANIC("assertion failed: %s", #cond); } while (0)

#endif
