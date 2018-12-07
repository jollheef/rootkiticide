TARGET := rkcd

KERNEL := /lib/modules/$(shell uname -r)/build
VMHOST := qemu

obj-m += $(TARGET).o
$(TARGET)-objs = rootkiticide.o
$(TARGET)-objs +=  scheduler_hook.o fd_hook.o hw_breakpoint.o proc.o ringbuf.o
ccflags-y := -std=gnu99 -Wno-declaration-after-statement -Wall
ccflags-y += -Wframe-larger-than=8192 # it's safe or not?

module:
	make -C $(KERNEL) M=$(PWD) modules

cli:
	go build rkcdcli.go

clean:
	make -C $(KERNEL) M=$(PWD) clean
	rm -f rkcdcli

vm-insmod: all
	scp {*.ko,rkcdcli} "$(VMHOST):"
	ssh $(VMHOST) "rmmod *.ko; insmod *.ko"

all: module cli
