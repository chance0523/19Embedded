
CC = arm-linux-gcc
obj-m := dipsw.o
KDIR := /home/parallels/working/linux-2.6.32-hanback
PWD := $(shell pwd)
TEST_TARGET = dip_test 
TEST_SRCS = $(TEST_TARGET).c
default:$(TEST_TARGET)
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules
	$(CC) $(TEST_SRCS) -o $@
	rm -f default
clean:
	rm -f *.ko
	rm -f *.o
	rm -f *.mod.*
	rm -f .*.cmd
	rm -rf
	$(TEST_TARGET) Module.* module* .tmp_versions
