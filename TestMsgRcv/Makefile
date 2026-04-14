CC = gcc
CFLAGS = -Wall -Wextra -g -pthread -Iinclude -I../SHM/include -MMD -MP $(IFDEF)
TARGET = TestMsgRcv
SRCDIR = src
OBJDIR = obj
SRCS = \
	$(SRCDIR)/main.c \
	$(SRCDIR)/udp_worker.c \
	$(SRCDIR)/signal_worker.c \
	$(SRCDIR)/msg_common.c \
	$(SRCDIR)/log_wrapper.c \
	$(SRCDIR)/shm_worker.c
OBJS = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SRCS))
#IFDEF = -DENABLE_FAULT_INJECTION
LIBDIRNAME  = ../lib
LDFLAGS     = -L$(LIBDIRNAME)
LDLIBS      = -lshm

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
	src/msg_common.c \
	src/log_wrapper.c
	$(CC) $(CFLAGS) -o $@ $^
