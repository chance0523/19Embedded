
CC = arm-linux-gcc
APP = facedetect
CFLAGS = -DNO_DEBUG -I/usr/OpenCV_1_1_0_arm/include/opencv/ -L/usr/OpenCV_1_1_0_arm/lib/ -lcxcore -lcv -lhighgui -lcvaux -lml
OBJS = $(APP).o
all: $(APP)
$(APP): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^
clean:
	rm -f $(OBJS) $(APP)


