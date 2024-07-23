# name of object file
obj-m := vl53l0x.o 

# cross compilation env vars
ARCH := arm64
CROSS_COMPILE := aarch64-poky-linux-

# Set KDIR to kernel source dir
KDIR := /home/embedded/linux-$(KERNEL_VERSION)
PWD := $(shell pwd)

all:
	make  ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) \
				-C $(KDIR) M=$(PWD) modules

