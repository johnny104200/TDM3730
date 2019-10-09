
#KVERSION := $(shell uname -r)
#KERNEL_DIR = /usr/src/linux-headers-$(KVERSION)/

MODULE_NAME = uarttest
PWD := $(shell pwd) 
obj-m := $(MODULE_NAME).o

all:
	make -C $(KERNEL_DIR) M=$(PWD) modules
clean:
	make -C $(KERNEL_DIR) M=$(PWD) clean
