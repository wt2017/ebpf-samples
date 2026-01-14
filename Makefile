obj-m += nvme_tlp_drop.o

KDIR ?= /lib/modules/$(shell uname -r)/build

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

install:
	sudo insmod nvme_tlp_drop.ko drop_rate=10

uninstall:
	sudo rmmod nvme_tlp_drop

