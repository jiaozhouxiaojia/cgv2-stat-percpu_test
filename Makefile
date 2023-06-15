# SPDX-License-Identifier: GPL-2.0-only

kernelver ?= $(shell uname -r)
obj-m += cgv2_stat_percpu.o

all:
	$(MAKE) -C /lib/modules/$(kernelver)/build M=$(CURDIR) modules
clean:
	$(MAKE) -C /lib/modules/$(kernelver)/build M=$(CURDIR) clean

