CC = gcc
CFLAGS = -std=c89 -pedantic -Wall -I/usr/X11R7/include -I/usr/X11/include
LDFLAGS = -L/usr/X11/lib64
LIBS = -lXaw -lXt -lXmu -lX11

SRCS = main.c wm.c ui.c
OBJS = $(SRCS:.c=.o)
TARGET = antiquity

all: $(TARGET)

debug: CFLAGS += -D_DEBUG
debug: clean $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

%.o: %.c antiquity.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean debug
