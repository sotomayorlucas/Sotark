CC       ?= gcc
CFLAGS   ?= -std=c99 -Wall -Wextra -Wpedantic -O2 -g
CFLAGS   += -I../common/include $(shell sdl2-config --cflags) -pthread -D_POSIX_C_SOURCE=200809L
LDFLAGS  := $(shell sdl2-config --libs) -lm -pthread
LDLIBS   := ../common/libcommon.a

SRCS   := src/main.c src/framebuffer.c src/draw_line.c src/draw_tri.c src/texture.c src/frustum.c src/bsp.c src/span_buffer.c src/rast_scan.c src/lightmap.c
OBJS   := $(SRCS:.c=.o)
TARGET := sw_renderer

.PHONY: all clean
all: $(TARGET)

$(TARGET): $(OBJS) ../common/libcommon.a
	$(CC) -o $@ $(OBJS) $(LDLIBS) $(LDFLAGS)

../common/libcommon.a:
	$(MAKE) -C ../common

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)
