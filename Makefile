CHIP?=stm32f103x8
ROMSIZE=65536

PLAT_FLAGS=-DSTM32F1 -mthumb -mcpu=cortex-m3 -msoft-float
VARIANT=stm32f1
BOARD=bluepill


CC=arm-none-eabi-gcc
LD=arm-none-eabi-ld
OBJCOPY=arm-none-eabi-objcopy
OBJDUMP=arm-none-eabi-objdump

BUILDDIR=build
BUILDBOARDDIR=$(BUILDDIR)/$(BOARD)

BINARY = main

PROJDIR=$(realpath .)
SRCDIR=./src

#PLAT_FLAGS=-DSTM32F3 -mthumb -mcpu=cortex-m4 -mhard-float
#VARIANT=stm32f3

MAIN_CFILES=src/main.c src/glyphs.c src/video.c src/mktime.c


OPENCM3_ROOT=/st/build/libopencm3
OPENCM3_LIBRARY=$(OPENCM3_ROOT)/lib/libopencm3_$(VARIANT).a
OPENCM3_INCLUDE=-I$(OPENCM3_ROOT)/include
OPENCM3_LDFLAGS=-L$(OPENCM3_ROOT)/lib $(OPENCM3_LIBRARY)


OBJS+=$(MAIN_CFILES:.c=.o)
# VPATH=./rtos:./$(RTOS_PORTDIR)

UNDEFINED_SYMBOLS+=-Wl,--undefined=rtc_set_counter_val


BUILD_OBJS=$(addprefix $(BUILDBOARDDIR)/, $(OBJS))

LDSCRIPT=$(wildcard $(OPENCM3_ROOT)/lib/stm32/*/$(CHIP).ld)
#LDSCRIPT=base.ld



CFLAGS+= -std=c99 -ggdb3
CFLAGS+= $(GCC_SPECS)
CFLAGS+= -Wall -Wundef
CFLAGS+= -Wextra -Wshadow -Wimplicit-function-declaration -Wredundant-decls -Wmissing-prototypes -Wstrict-prototypes
CFLAGS+= -fno-common -ffunction-sections -fdata-sections -fno-builtin
CFLAGS+= -MD -Os
#CFLAGS+= -DappUSE_HARD_FAULT_HANDLER=1
# listings
#CFLAGS+= -Wa,-adhln
CFLAGS+= -I$(realpath ./include)
CFLAGS+= $(OPENCM3_INCLUDE) \
        $(RTOS_INCLUDE) \
        $(BOARD_CONFIG) \
        $(PLAT_FLAGS) \
        $(TEMP_CFLAGS)

#CFLAGS+= -I$(realpath .) -I$(realpath ./include) -I$(COMMON_INCDIR) -include config.h


LDFLAGS=--static -nostartfiles -T$(LDSCRIPT) $(ADDITIONAL_LDSCRIPTS) $(GCC_SPECS) $(PLAT_FLAGS) -ggdb3 -Wl,-Map=$(BUILDBOARDDIR)/$(BINARY).map -Wl,--cref -Wl,--gc-sections -L$(OPENCM3_ROOT)/lib $(UNDEFINED_SYMBOLS)

LIBS= $(OPENCM3_LIBRARY) -lm



all: $(BUILDBOARDDIR)/$(BINARY).bin stats

$(BUILDBOARDDIR)/$(BINARY).elf $(BUILDBOARDDIR)/$(BINARY).map: $(BUILD_OBJS) $(XBUILD_OBJS) $(STARTFILE_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS) $(POST_LDFLAGS)

$(BUILDBOARDDIR)/$(BINARY).hex: $(BUILDBOARDDIR)/$(BINARY).elf
	$(OBJCOPY) -O ihex $< $@

$(BUILDBOARDDIR)/$(BINARY).bin: $(BUILDBOARDDIR)/$(BINARY).elf
	$(OBJCOPY) -O binary $< $@

$(BUILD_OBJS): $(BUILDBOARDDIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -o $@ -c $<



stats:
	@arm-none-eabi-objdump -t $(BUILDBOARDDIR)/$(BINARY).elf | sort > $(BUILDBOARDDIR)/$(BINARY).nm
	@arm-none-eabi-size `find $(BUILDBOARDDIR) -name \*.o` $(BUILDBOARDDIR)/$(BINARY).elf

clean:
	rm -rf $(BUILDDIR)

qclean:
	rm -rf $(BUILDBOARDDIR)

.PHONY: clean all program stats

