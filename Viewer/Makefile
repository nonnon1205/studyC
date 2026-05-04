# 共通環境の読み込み
-include ../env.mk

# モジュール固有のコンパイルオプション
override CFLAGS += -pthread -Iinclude -I../lib/include $(IFDEF)
SRCDIR  = src
OBJDIR  = obj
TARGET  = Viewer
SRCS    = $(SRCDIR)/main.c $(SRCDIR)/udp_worker.c $(SRCDIR)/signal_thread.c $(SRCDIR)/tcp_worker.c $(SRCDIR)/mgmt_handlers.c
OBJS    = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SRCS))
LDFLAGS = -L../lib
LDLIBS  = -lmgmt -lshm -lcommon

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
