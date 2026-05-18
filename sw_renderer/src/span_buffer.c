#include "span_buffer.h"

#include <stdlib.h>
#include <string.h>

bool span_buffer_init(span_buffer_t *sb, int w, int h) {
    sb->covered = (u8 *)malloc((size_t)w * (size_t)h);
    if (!sb->covered) return false;
    sb->w = w;
    sb->h = h;
    span_buffer_clear(sb);
    return true;
}

void span_buffer_free(span_buffer_t *sb) {
    free(sb->covered);
    sb->covered = NULL;
}

void span_buffer_clear(span_buffer_t *sb) {
    memset(sb->covered, 0, (size_t)sb->w * (size_t)sb->h);
    sb->pixels_drawn = 0;
    sb->pixels_skipped = 0;
}

void span_buffer_emit(span_buffer_t *sb, int y, int x0, int x1,
                      span_draw_fn fn, void *ud) {
    if (y < 0 || y >= sb->h) return;
    if (x0 < 0) x0 = 0;
    if (x1 > sb->w) x1 = sb->w;
    if (x0 >= x1) return;

    u8 *row = sb->covered + (size_t)y * sb->w;
    int run_start = -1;
    for (int x = x0; x < x1; ++x) {
        if (!row[x]) {
            if (run_start < 0) run_start = x;
            row[x] = 1;
        } else {
            sb->pixels_skipped++;
            if (run_start >= 0) {
                fn(y, run_start, x, ud);
                sb->pixels_drawn += x - run_start;
                run_start = -1;
            }
        }
    }
    if (run_start >= 0) {
        fn(y, run_start, x1, ud);
        sb->pixels_drawn += x1 - run_start;
    }
}
