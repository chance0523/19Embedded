CC = arm-linux-gcc
APP = fp_test
CFLAGS = -DNO_DEBUG -I/home/jongseo/working/OpenCV_1_1_0_arm/include/opencv/ -L/home/jongseo/working/OpenCV_1_1_0_arm/lib/ -lcxcore -lcv -lhighgui -lcvaux -lml

OBJS = $(APP).o

all: $(APP)

$(APP): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(OBJS) $(APP)


