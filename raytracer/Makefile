CC      ?= gcc
CFLAGS  ?= -std=c99 -Wall -Wextra -Wpedantic -O2 -g
CFLAGS  += -Iinclude -I../common/include -pthread
LDLIBS  := ../common/libcommon.a -lm -pthread

SRCS   := src/main.c src/hittable.c src/material.c src/camera.c src/bvh.c src/obj.c
OBJS   := $(SRCS:.c=.o)
TARGET := raytracer

.PHONY: all clean
all: $(TARGET)

$(TARGET): $(OBJS) ../common/libcommon.a
	$(CC) -o $@ $(OBJS) $(LDLIBS)

../common/libcommon.a:
	$(MAKE) -C ../common

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)
