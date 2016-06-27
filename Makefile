BINARY = myblink
OPENCM3_DIR = libopencm3
LDSCRIPT = stm32f103c8.ld
OOCD_INTERFACE = stlink-v2-1
OOCD_BOARD = st_nucleo_f103rb



LIBNAME         = opencm3_stm32f1
DEFS            += -DSTM32F1

FP_FLAGS        ?= -msoft-float
ARCH_FLAGS      = -mthumb -mcpu=cortex-m3 $(FP_FLAGS) -mfix-cortex-m3-ldrd

STLINK_PORT    ?= :4242


include Makefile.include

flash: foo.stlink-flash


