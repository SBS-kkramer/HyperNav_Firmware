.DEFAULT:
.SUFFIXES:
.PRECIOUS: %.o
.PHONY: force
SHELL=/bin/csh
CFLAGS= -gstabs -I. -Wall -Wno-implicit
C++FLAGS= -x c++ -gstabs+ -I. -Wall -Wno-implicit
C++LIBS= -lstdc++ 
SYSLIBS= -lm -lutil

rudics: rudics.o logger.o serial.o cstring.o socket.o login.o expect.o \
        chat.o upload.o tx.o pkt.o xmodem.o download.o rx.o crc16bit.o
	gcc  $^ $(LIBS) $(OBJS) $(CLIBS) $(C++LIBS) $(CLIBS) $(C++LIBS) $(SYSLIBS) $(CFLAGS) $(LIBS) -o rudics

%: %.o $(OBJS)
	gcc  $*.o $(LIBS) $(OBJS) $(CLIBS) $(C++LIBS) $(CLIBS) $(C++LIBS) $(SYSLIBS) $(CFLAGS) $(LIBS) -o $*

%.o: %.c
	gcc -D$(shell echo $*.c|tr a-z.- A-Z__) -Dpersistent= -Dfar= -Dcode= -c -x c \
	    -gstabs -I. -ansi -pedantic-errors -Wall $< -o $@

%.o: %.cc
	gcc -c $(C++FLAGS) -DUSE_LIBGXX_INLINES $*.cc -o $*.o 

%.o: %.cpp
	gcc -D$(shell echo $*.cpp|tr a-z.- A-Z__) -c $(C++FLAGS) $*.cpp -o $*.o 

%.cpp: %.l
	flex -o$*.cpp $*.l

empty:
	@echo Make argument missing...

clean: force
	-rm -f rudics rudics.o logger.o serial.o cstring.o socket.o login.o \
       expect.o chat.o upload.o tx.o pkt.o xmodem.o download.o rx.o crc16bit.o
