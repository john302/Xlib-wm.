CC = gcc
CFLAGS = -Wall -Wextra
LDFLAGS = -lX11

TARGET = xlib-wm
SRC = xlib-wm.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
