CXX=g++
RM=rm -f
CPPFLAGS=-O2 -Wall -Werror
LDFLAGS=-O2 -Wall -Werror
LDLIBS=-lrt

SRCS=main.cpp crypt_stuff.cpp standard_stuff.cpp socket_stuff.cpp
OBJS=$(subst .cpp,.o,$(SRCS))

all: stickszerver

stickszerver: $(OBJS)
	$(CXX) $(LDFLAGS) -o stickszerver $(OBJS) $(LDLIBS)

main.o: crypt_stuff.h standard_stuff.h socket_stuff.h

standard_stuff.o: standard_stuff.h

socket_stuff.o: socket_stuff.h

crypt_stuff.o: crypt_stuff.h

clean:
	$(RM) $(OBJS) stickszerver
