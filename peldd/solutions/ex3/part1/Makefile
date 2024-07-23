# name of object file
obj-m := jhd1313.o 

# cross compilation env vars
ARCH := arm64
CROSS_COMPILE := aarch64-poky-linux-
ccflags-y += -Wno-date-time

# Set KDIR to kernel source dir
KDIR := ${HOME}/linux-${KERNEL_VERSION}
PWD := $(shell pwd)

all:
	make  ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KDIR)  M=$(PWD) modules

