CC = arm-linux-gcc
<<<<<<< HEAD
APP = term
=======
APP = fp_test
>>>>>>> 441810bd256e0dede1730cddb1ba0e5404f12c67
CFLAGS = -DNO_DEBUG -I/home/chance/19em/working/opencv_test/OpenCV_1_1_0_arm/include/opencv/ -L/home/chance/19em/working/opencv_test/OpenCV_1_1_0_arm/lib/ -lcxcore -lcv -lhighgui -lcvaux -lml

OBJS = $(APP).o

all: $(APP)

$(APP): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(OBJS) $(APP)


