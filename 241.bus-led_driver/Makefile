
CC = arm-linux-gcc
obj-m := ledioport.o
KDIR := /home/parallels/working/linux-2.6.32-hanback
PWD := $(shell pwd)
TEST_TARGET = ledtest
TEST_SRCS = $(TEST_TARGET).c
default:$(TEST_TARGET)
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules
	$(CC) $(TEST_SRCS) -o $(TEST_TARGET)
	rm -f default
clean:
	rm -f *.ko
	rm -f *.o
	rm -f *.mod.*
	rm -f .*.cmd
	rm -rf 
	$(TEST_TARGET) Module.* module* .tmp_versions Module.* 
	module* .tmp_versions
