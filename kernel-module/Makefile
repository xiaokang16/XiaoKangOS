obj-m += xiao_syscall.o
xiao_syscall-objs := main.o xiao_fs.o xiao_proc.o xiao_sys.o xiao_security.o xiao_net.o xiao_hardware.o xiao_ipc.o

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

install:
	$(MAKE) -C $(KDIR) M=$(PWD) modules_install

help:
	$(MAKE) -C $(KDIR) M=$(PWD) help
