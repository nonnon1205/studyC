CC = gcc
CFLAGS = -Wall -Wextra -g -pthread -Iinclude -D_GNU_SOURCE -D_POSIX_C_SOURCE=200809L
SRCDIR = src
OBJDIR = obj
INCDIR_LOCAL = include

# Source files for Mgmt module
SRCS = $(SRCDIR)/mgmt_protocol.c \
       $(SRCDIR)/event_handler.c \
       $(SRCDIR)/unified_logger.c \
       $(SRCDIR)/metrics_collector.c \
       $(SRCDIR)/mgmt_socket.c

OBJS = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SRCS))
HEADERS = $(INCDIR_LOCAL)/mgmt_protocol.h \
          $(INCDIR_LOCAL)/event_handler.h \
          $(INCDIR_LOCAL)/unified_logger.h \
          $(INCDIR_LOCAL)/metrics_collector.h \
          $(INCDIR_LOCAL)/mgmt_socket.h

# Installation targets
PREFIX = $(abspath ..)
LIB_INCDIR = $(PREFIX)/lib/include
LIB_SRCDIR = $(PREFIX)/lib/src
TARGET_LIB = $(PREFIX)/lib/libmgmt.a

.PHONY: all install install-headers install-sources clean

all: $(TARGET_LIB)

install: all install-headers install-sources

install-headers: | $(LIB_INCDIR)
	@echo "[Mgmt] Installing headers to $(LIB_INCDIR)..."
	@install -m 644 $(HEADERS) $(LIB_INCDIR)/

install-sources: | $(LIB_SRCDIR)
	@echo "[Mgmt] Installing source files to $(LIB_SRCDIR)..."
	@install -m 644 $(SRCS) $(LIB_SRCDIR)/

$(LIB_INCDIR):
	@mkdir -p $(LIB_INCDIR)

$(LIB_SRCDIR):
	@mkdir -p $(LIB_SRCDIR)

$(TARGET_LIB): $(OBJS)
	@echo "[Mgmt] Creating library archive: $@"
	@ar rcs $@ $^
	@echo "[Mgmt] Built: $@"

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	@echo "[Mgmt] Compiling: $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR):
	@mkdir -p $(OBJDIR)

clean:
	@echo "[Mgmt] Cleaning..."
	@rm -rf $(OBJDIR) $(TARGET_LIB)
	@echo "[Mgmt] Cleaned"

print-vars:
	@echo "SRCS: $(SRCS)"
	@echo "OBJS: $(OBJS)"
	@echo "TARGET_LIB: $(TARGET_LIB)"
	@echo "LIB_INCDIR: $(LIB_INCDIR)"
