KERNEL := /lib/modules/$(shell uname -r)/build
VMHOST := qemu

obj-m += rkcd.o
rkcd-objs = rootkiticide.o
ccflags-y := -std=gnu99 -Wno-declaration-after-statement

all:
	make -C $(KERNEL) M=$(PWD) modules

clean:
	make -C $(KERNEL) M=$(PWD) clean

vm-insmod: all
	scp *.ko "$(VMHOST):"
	ssh $(VMHOST) "rmmod *.ko; insmod *.ko"
