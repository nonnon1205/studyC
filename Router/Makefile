# 共通環境の読み込み
-include ../env.mk

# モジュール固有のコンパイルオプション
override CFLAGS += -pthread -Iinclude -I../lib/include -MMD -MP $(IFDEF)
TARGET = Router
SRCDIR = src
OBJDIR = obj
SRCS = \
	$(SRCDIR)/main.c \
	$(SRCDIR)/udp_worker.c \
	$(SRCDIR)/signal_worker.c \
	$(SRCDIR)/router_worker.c \
	$(SRCDIR)/msg_common.c \
	$(SRCDIR)/shm_worker.c
OBJS = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SRCS))
#IFDEF = -DENABLE_FAULT_INJECTION
LIBDIRNAME  = ../lib
LDFLAGS     = -L$(LIBDIRNAME)
LDLIBS      = -lshm -lcommon

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS) $(LDLIBS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR):
	mkdir -p $(OBJDIR)

clean:
	rm -rf $(OBJDIR) $(TARGET) test_common

-include $(OBJS:.o=.d)

test: test_common
	./test_common

test_common: \
	tests/test_common.c \
	src/msg_common.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)
