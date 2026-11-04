obj-m := vaccel.o
ccflags-y += -I$(src)/../include

KDIR ?= /lib/modules/$(shell uname -r)/build
KBUILD_FLAGS ?= $(if $(shell grep -s '^CONFIG_CC_IS_CLANG=y' $(KDIR)/include/config/auto.conf),LLVM=1)

.PHONY: all clean
all:
	$(MAKE) -C $(KDIR) M=$(CURDIR) $(KBUILD_FLAGS) modules

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) $(KBUILD_FLAGS) clean
