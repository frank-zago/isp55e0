CC ?= gcc
CFLAGS = -O2 -Wall -Werror
LDLIBS = -lusb-1.0

all: isp55e0

clean:
	rm -f isp55e0 *.o
