CC      ?= gcc
AR      ?= ar
CFLAGS  ?= -std=c99 -Wall -Wextra -Wpedantic -O2 -g
CFLAGS  += -Iinclude

SRCS    := src/image.c src/mat.c src/rng.c
OBJS    := $(SRCS:.c=.o)
TARGET  := libcommon.a

.PHONY: all clean
all: $(TARGET)

$(TARGET): $(OBJS)
	$(AR) rcs $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)
