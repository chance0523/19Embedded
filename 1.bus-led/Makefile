CC = arm-linux-gcc
CFLAGS = -DNO_DEBUG
EXEC=led
OBJS=$(EXEC).o
####### Implicit rules 
.SUFFIXES:.cpp .cxx .cc .C .c
.cpp.o:
	$(CXX) -c $(CXXFLAGS) -o $@ $< 
.cxx.o:
	$(CXX) -c $(CXXFLAGS) -o $@ $<
.cc.o:
	$(CXX) -c $(CXXFLAGS) -o $@ $<
.C.o:
	$(CXX) -c $(CXXFLAGS) -o $@ $<
.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<
all: $(EXEC)
	$(EXEC) : $(OBJS)
	$(CC) -o $@ $^
clean:
	rm - f $(OBJS) $(EXEC)