obj-m = hello0.o
hello0-objs := nctu_br_udp20191007.o util.o send_func.o uart.o

PWD = $(shell pwd)

KBUILD_EXTRA_SYMBOLS +=/home/lab818/Documents/Theory/2010_0709_RT2870_Linux_STA_v2.4.0.1/os/linux/Module.symvers
export KBUILD_EXTRA_SYMBOLS

all:
		#make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
		#make -C /root/tdm-20110527/kernel/TAO-PSP-03000106 M=$(PWD) modules
		make -C /ARM/tdm-20110527/kernel/TAO-PSP-03000106 M=$(PWD) modules
clean:
		#make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
		#make -C /root/tdm-20110527/kernel/TAO-PSP-03000106 M=$(PWD) clean
		make -C /ARM/tdm-20110527/kernel/TAO-PSP-03000106 M=$(PWD) clean

