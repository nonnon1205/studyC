-include ../env.mk

override CFLAGS += -std=c17 -D_POSIX_C_SOURCE=200809L -pthread \
                   -Wall -Wextra -Iinclude -I../lib/include $(IFDEF)
BUILDDIR = ../build
TARGET   = $(BUILDDIR)/mgmtctl
SRCDIR   = src
OBJDIR   = obj
SRCS    = $(SRCDIR)/main.c $(SRCDIR)/mgmt_send.c
OBJS    = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SRCS))
LDFLAGS = -L../lib
LDLIBS  = -lmgmt -lcommon -lpthread

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
	rm -rf $(OBJDIR) $(TARGET)
