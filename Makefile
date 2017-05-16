KERNEL := /lib/modules/$(shell uname -r)/build
VMHOST := qemu

obj-m += rkcd.o
rkcd-objs = rootkiticide.o scheduler_hook.o fd_hook.o hw_breakpoint.o
ccflags-y := -std=gnu99 -Wno-declaration-after-statement -Wall
ccflags-y += -Wframe-larger-than=8192 # it's safe or not?

all:
	make -C $(KERNEL) M=$(PWD) modules

clean:
	make -C $(KERNEL) M=$(PWD) clean

vm-insmod: all
	scp *.ko "$(VMHOST):"
	ssh $(VMHOST) "rmmod *.ko; insmod *.ko"
