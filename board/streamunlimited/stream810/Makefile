# (C) Copyright 2015 Freescale Semiconductor, Inc.
#
# SPDX-License-Identifier:	GPL-2.0+
#

obj-y  := board.o

extra-$(CONFIG_USE_PLUGIN) :=  plugin_4Gb.bin plugin_2Gb.bin
$(obj)/plugin_2Gb.bin: $(obj)/plugin_2Gb.o
	$(OBJCOPY) -O binary --gap-fill 0xff $< $@

$(obj)/plugin_4Gb.bin: $(obj)/plugin_4Gb.o
	$(OBJCOPY) -O binary --gap-fill 0xff $< $@
