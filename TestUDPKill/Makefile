# 共通環境の読み込み
-include ../env.mk

# モジュール固有のコンパイルオプション
override CFLAGS += -pthread -Iinclude -I../lib/include
SRCDIR  = src
OBJDIR  = obj
TARGET  = threadTest
SRCS    = $(SRCDIR)/main.c $(SRCDIR)/udp_worker.c $(SRCDIR)/ipc_worker.c $(SRCDIR)/signal_thread.c $(SRCDIR)/tcp_worker.c
OBJS    = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SRCS))
LDFLAGS = -L../lib
LDLIBS  = -lshm -lcommon

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS) $(LDLIBS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR):
	mkdir -p $(OBJDIR)

clean:
	rm -rf $(OBJDIR) $(TARGET)
