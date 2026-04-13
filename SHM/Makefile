CC = gcc
CFLAGS = -Wall -Wextra -g -pthread -Iinclude
SRCDIR = src
OBJDIR = obj
SRCS = $(SRCDIR)/shm_api.c
OBJS = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SRCS))
TARGET = /home/$(USER)/sutdyC/lib/libshm.a

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	ar rcs $@ $^

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR):
	mkdir -p $(OBJDIR)

clean:
	rm -rf $(OBJDIR) $(TARGET)
