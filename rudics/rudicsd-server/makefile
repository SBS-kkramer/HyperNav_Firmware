.DEFAULT:
.SUFFIXES:
.PRECIOUS: %.o
SHELL=/bin/csh
C++FLAGS= -x c++ -gstabs+ -I. -Wall -Wno-implicit 
C++LIBS= -lstdc++ 
SYSLIBS= -lm -lutil

rudicsd: rudicsd.o 
	gcc  $^  $(C++LIBS) $(C++LIBS) $(SYSLIBS) -o rudicsd

%: %.o 
	gcc  $*.o $(C++LIBS) $(C++LIBS) $(SYSLIBS) -o $*

%.o: %.cpp
	gcc -D$(shell echo $*.cpp|tr a-z.- A-Z__) -c $(C++FLAGS) $*.cpp -o $*.o 

empty:
	@echo Make argument missing...

clean:
	rm rudicsd{,.o}
