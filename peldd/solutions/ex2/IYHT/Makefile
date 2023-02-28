# name of object file
obj-m := simpledeps.o testdeps.o

# cross compilation env vars
ARCH := arm64
CROSS_COMPILE := aarch64-linux-gnu-

# Set KDIR to kernel source dir
KDIR := /home/builduser/linux-$(KERNEL_VERSION)/
PWD := $(shell pwd)
ccflags-y += -Wno-date-time

all:
	make  ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) \
				-C $(KDIR) M=$(PWD) modules

