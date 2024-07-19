CC ?= gcc
CFLAGS = -O2 -Wall -Werror
LDLIBS = -lusb-1.0

all: isp55e0

isp55e0.o: isp55e0.c chips.h compat-err.h

chips:
	./parse_wcfg.py > chips.h

clean:
	rm -f isp55e0 *.o
