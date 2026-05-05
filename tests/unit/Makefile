ROOT    = ../..
UNITY   = $(ROOT)/tests/unity
CC      = gcc
CFLAGS  = -std=c17 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -I$(UNITY) -I$(ROOT)/Mgmt/include

TESTS   = mgmt/test_mgmt_protocol

.PHONY: all clean

all: $(TESTS)
	@echo ""
	@for t in $(TESTS); do ./$$t; echo ""; done

mgmt/test_mgmt_protocol: mgmt/test_mgmt_protocol.c \
                          $(ROOT)/Mgmt/src/mgmt_protocol.c \
                          $(UNITY)/unity.c
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(TESTS)
