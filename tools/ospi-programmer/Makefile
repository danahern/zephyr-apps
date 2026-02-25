ZEPHYR_SDK ?= $(HOME)/zephyr-sdk-0.17.4
TOOLCHAIN = $(ZEPHYR_SDK)/arm-zephyr-eabi/bin/arm-zephyr-eabi

CC = $(TOOLCHAIN)-gcc
AS = $(TOOLCHAIN)-gcc
LD = $(TOOLCHAIN)-gcc
OBJCOPY = $(TOOLCHAIN)-objcopy
SIZE = $(TOOLCHAIN)-size

CFLAGS = -mcpu=cortex-m33 -mthumb -mfloat-abi=soft \
         -Os -Wall -Wextra -Werror \
         -ffreestanding -nostdlib \
         -Isrc -Ilib
ASFLAGS = -mcpu=cortex-m33 -mthumb -mfloat-abi=soft
LDFLAGS = -mcpu=cortex-m33 -mthumb -mfloat-abi=soft \
          -nostdlib -Tlinker.ld -Wl,--gc-sections -Wl,-Map=build/ospi_programmer.map

BUILD = build

SRCS_C = src/main.c src/ospi_drv.c src/ospi_flash.c src/crc32.c src/libc_stubs.c lib/SEGGER_RTT.c
SRCS_S = src/startup.S
OBJS = $(patsubst %.c,$(BUILD)/%.o,$(SRCS_C)) $(patsubst %.S,$(BUILD)/%.o,$(SRCS_S))

.PHONY: all clean size

all: $(BUILD)/ospi_programmer.bin size

$(BUILD)/ospi_programmer.bin: $(BUILD)/ospi_programmer.elf
	$(OBJCOPY) -O binary $< $@

$(BUILD)/ospi_programmer.elf: $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $^

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/%.o: %.S
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) -c -o $@ $<

size: $(BUILD)/ospi_programmer.elf
	$(SIZE) $<

clean:
	rm -rf $(BUILD)
