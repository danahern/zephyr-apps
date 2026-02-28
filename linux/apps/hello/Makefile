# Cross-compilation Makefile for Linux Cortex-A apps
#
# BOARD selects the target:
#   (default)      STM32MP1 Cortex-A7 via Buildroot toolchain
#   alif-e7/e8     Alif Ensemble Cortex-A32 via system cross-compiler

ifneq ($(filter alif-%,$(BOARD)),)
  # System cross-compiler (crossbuild-essential-armhf)
  CROSS_COMPILE ?= arm-linux-gnueabihf-
  CFLAGS  = -Wall -O2 -mcpu=cortex-a32 -mfpu=neon -mfloat-abi=hard
else
  # Buildroot toolchain path (inside container after build)
  TOOLCHAIN_DIR ?= /home/builder/buildroot/output/host/bin
  CROSS_COMPILE ?= $(TOOLCHAIN_DIR)/arm-none-linux-gnueabihf-
  CFLAGS  = -Wall -O2 -mcpu=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard
endif

CC      = $(CROSS_COMPILE)gcc
STRIP   = $(CROSS_COMPILE)strip
LDFLAGS =

TARGET  = hello
SRCS    = main.c
OBJS    = $(SRCS:.c=.o)

ARTIFACTS_DIR ?= /artifacts

.PHONY: all clean install

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^
	$(STRIP) $@

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

install: $(TARGET)
	cp $(TARGET) $(ARTIFACTS_DIR)/

clean:
	rm -f $(TARGET) $(OBJS)
