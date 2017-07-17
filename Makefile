KERNEL := /lib/modules/$(shell uname -r)/build
VMHOST := qemu

obj-m += rkcd.o
rkcd-objs = rootkiticide.o
rkcd-objs +=  scheduler_hook.o fd_hook.o hw_breakpoint.o proc.o ringbuf.o
ccflags-y := -std=gnu99 -Wno-declaration-after-statement -Wall
ccflags-y += -Wframe-larger-than=8192 # it's safe or not?

all:
	make -C $(KERNEL) M=$(PWD) modules
	go build rkcdcli.go

clean:
	make -C $(KERNEL) M=$(PWD) clean
	rm -f rkcdcli

vm-insmod: all
	scp {*.ko,rkcdcli} "$(VMHOST):"
	ssh $(VMHOST) "rmmod *.ko; insmod *.ko"
