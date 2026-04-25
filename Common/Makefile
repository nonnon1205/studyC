CC = gcc
CFLAGS = -Wall -Wextra -g -pthread -Iinclude
SRCDIR = src
OBJDIR = obj
SRCS = $(wildcard $(SRCDIR)/*.c)
OBJS = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SRCS))
PREFIX = $(abspath ..)
INCDIR = $(PREFIX)/lib/include
TARGET = $(PREFIX)/lib/libcommon.a

.PHONY: all install install-headers clean

all: $(TARGET)

install: all install-headers

install-headers: | $(INCDIR)
	if [ -d include ]; then cp -a include/. $(INCDIR)/; fi

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