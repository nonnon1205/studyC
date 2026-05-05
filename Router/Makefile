# 共通環境の読み込み
-include ../env.mk

# モジュール固有のコンパイルオプション
override CFLAGS += -std=c17 -pthread -Iinclude -I../lib/include -MMD -MP $(IFDEF)
BUILDDIR    = ../build
TARGET      = $(BUILDDIR)/Router
SRCDIR      = src
OBJDIR      = obj
SRCS = \
	$(SRCDIR)/main.c \
	$(SRCDIR)/udp_worker.c \
	$(SRCDIR)/signal_worker.c \
	$(SRCDIR)/router_worker.c \
	$(SRCDIR)/msg_common.c \
	$(SRCDIR)/mgmt_handlers.c
OBJS = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SRCS))
#IFDEF = -DENABLE_FAULT_INJECTION
LIBDIRNAME  = ../lib
LDFLAGS     = -L$(LIBDIRNAME)
LDLIBS      = -lmgmt -lshm -lcommon

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS) | $(BUILDDIR)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS) $(LDLIBS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

clean:
	rm -rf $(OBJDIR) $(TARGET) test_common

-include $(OBJS:.o=.d)

test: test_common
	./test_common

test_common: \
	tests/test_common.c \
	src/msg_common.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)
