# 変数の定義
CC      = gcc
CFLAGS  = -Wall -Wextra -g -pthread -Iinclude -I../lib/include
TARGET  = threadTest
SRCS    = src/main.c src/udp_worker.c src/ipc_worker.c src/signal_thread.c src/tcp_worker.c
LDFLAGS = -L../lib
LDLIBS  = -lshm

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRCS)
	@echo "Compiling: $(CC) $(CFLAGS) -o $(TARGET) $(SRCS) $(LDFLAGS) $(LDLIBS)"
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS) $(LDFLAGS) $(LDLIBS)

clean:
	rm -f $(TARGET)
