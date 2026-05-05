# 共通環境の読み込み
-include ../env.mk

# モジュール固有のコンパイルオプション
override CFLAGS += -std=c17 -pthread -D_POSIX_C_SOURCE=200809L -Iinclude -I../lib/include
SRCDIR    = src
OBJDIR    = obj
BUILDDIR  = ../build
TARGET    = $(BUILDDIR)/Collector
SRCS = $(SRCDIR)/main.c $(SRCDIR)/poll_io.c $(SRCDIR)/mgmt_handlers.c
OBJS = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SRCS))
LIBDIRNAME = ../lib
LDFLAGS = -L$(LIBDIRNAME)
LDLIBS = -lmgmt -lshm -lcommon

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS) $(LIBDIRNAME)/libshm.a | $(BUILDDIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

clean:
	rm -rf $(OBJDIR) $(TARGET)
