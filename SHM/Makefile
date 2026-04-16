CC = gcc
CFLAGS = -Wall -Wextra -g -pthread -Iinclude
SRCDIR = src
OBJDIR = obj
SRCS = $(SRCDIR)/shm_api.c
OBJS = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SRCS))
PREFIX = $(abspath ..)
INCDIR = $(PREFIX)/lib/include
TARGET = $(PREFIX)/lib/libshm.a

.PHONY: all install install-headers clean

all: $(TARGET)

install: all install-headers

install-headers: | $(INCDIR)
	install -m 644 include/shared_ipc.h $(INCDIR)/
	install -m 644 include/shm_api.h $(INCDIR)/

$(INCDIR):
	mkdir -p $(INCDIR)

$(TARGET): $(OBJS)
	ar rcs $@ $^

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR):
	mkdir -p $(OBJDIR)

clean:
	rm -rf $(OBJDIR) $(TARGET)
