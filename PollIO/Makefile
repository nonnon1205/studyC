CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g -pthread -D_POSIX_C_SOURCE=200809L
INCLUDES = -Iinclude -I../lib/include
SRCDIR = src
TARGET = pollio
SRCS = $(SRCDIR)/main.c $(SRCDIR)/poll_io.c
OBJS = $(SRCS:.c=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^

$(SRCDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

clean:
	rm -f $(SRCDIR)/*.o $(TARGET)
