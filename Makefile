TARGET = isp55e0
SRCS = isp55e0.c
OBJS = $(SRCS:.c=.o)

CC ?= gcc
WITHOUT_USB ?= 0

ifeq ($(WITHOUT_USB), 0)
LDLIBS = -lusb-1.0
endif

CFLAGS = -O2 -Wall -Werror
ifeq ($(WITHOUT_USB), 1)
CFLAGS += -DWITHOUT_USB=${WITHOUT_USB}
endif

.PHONY: all clean chips

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $(TARGET) $(OBJS) $(CFLAGS) $(LDLIBS)

.c.o:
	$(CC) $(CFLAGS)  -c $< -o $@

chips:
	./parse_wcfg.py > chips.h

clean:
	rm -f isp55e0 *.o
