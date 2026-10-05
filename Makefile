# 🕷️ Spider Audio - Native pactl compatibility tool & stack
# Copyright (c) 2026 Spider OS Contributors. Licensed under MIT License.

CC ?= gcc
CFLAGS ?= -O2 -Wall -Wextra -Ivendor/tinyalsa/include -Iinclude
TARGET = spider-pactl

TINYALSA_SRCS = vendor/tinyalsa/src/mixer.c \
                vendor/tinyalsa/src/mixer_hw.c \
                vendor/tinyalsa/src/pcm.c \
                vendor/tinyalsa/src/pcm_hw.c \
                vendor/tinyalsa/src/limits.c \
                vendor/tinyalsa/src/snd_card_plugin.c

SRCS = src/spider-pactl.c $(TINYALSA_SRCS)
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) src/*.o vendor/tinyalsa/src/*.o

install: $(TARGET)
	install -d /usr/local/bin
	install -m 755 $(TARGET) /usr/local/bin/spider-pactl
	ln -sf /usr/local/bin/spider-pactl /usr/local/bin/pactl
	install -d /etc
	install -m 644 conf/asound.conf /etc/asound.conf
	if [ -d /etc/init.d ]; then \
		install -d /etc/init.d; \
		install -m 755 init/spider-audio /etc/init.d/spider-audio; \
	fi

.PHONY: all clean install
