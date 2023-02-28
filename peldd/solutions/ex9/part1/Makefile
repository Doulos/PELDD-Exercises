ccflags-y += -Wno-date-time
obj-m += jhd1313.o


# Set KDIR to kernel source dir
KDIR := /home/builduser/linux-$(KERNEL_VERSION)
PWD := $(shell pwd)

#set ARCH to appropriate architecture
ARCH := arm64

#set CROSS_COMPILE to cross compiler prefix for toolchain
CROSS_COMPILE := aarch64-linux-gnu-

all:
	make ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) \
				-C $(KDIR) M=$(PWD) modules

