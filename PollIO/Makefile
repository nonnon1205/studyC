# 共通環境の読み込み
-include ../env.mk

# モジュール固有のコンパイルオプション
override CFLAGS += -std=c11 -pthread -D_POSIX_C_SOURCE=200809L -Iinclude -I../lib/include
SRCDIR = src
TARGET = pollio
SRCS = $(SRCDIR)/main.c $(SRCDIR)/poll_io.c
OBJS = $(SRCS:.c=.o)
LIBDIRNAME = ../lib
LDFLAGS = -L$(LIBDIRNAME)
LDLIBS = -lshm

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS) $(LIBDIRNAME)/libshm.a
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

$(SRCDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(SRCDIR)/*.o $(TARGET)
