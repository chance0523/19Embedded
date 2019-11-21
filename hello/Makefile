# Hello Device Driver Makefile

#CC= arm-linux-gcc-gnueabi
#CC=/home/parallels/working/arm-2009q3/bin/arm-none-linux-gnuebi-gcc
#CC=/home/parallels/working/arm-2009q3/bin/arm-linux-gcc
CC = arm-linux-gcc
obj-m := hello2.o

KDIR := /home/parallels/working/linux-2.6.32-hanback
#KDIR :=/working/linux-2.6.32-hanback

PWD := $(shell pwd)

default:

	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules

clean:
	rm -f *.ko
	rm -f *.o
	rm -f *.mod.*
	rm -f .*.cmd
