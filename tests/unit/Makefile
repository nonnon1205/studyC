ROOT         = ../..
UNITY        = $(ROOT)/tests/unity
CC           = gcc
CFLAGS       = -std=c17 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -I$(UNITY) -I$(ROOT)/Mgmt/include

TESTS        = mgmt/test_mgmt_protocol
COVERAGE_DIR = coverage

.PHONY: all clean coverage

all: $(TESTS)
	@echo ""
	@for t in $(TESTS); do ./$$t; echo ""; done

mgmt/test_mgmt_protocol: mgmt/test_mgmt_protocol.c \
                          $(ROOT)/Mgmt/src/mgmt_protocol.c \
                          $(UNITY)/unity.c
	$(CC) $(CFLAGS) -o $@ $^

coverage: CFLAGS += --coverage -O0
coverage: clean $(TESTS)
	@for t in $(TESTS); do ./$$t; echo ""; done
	mkdir -p $(COVERAGE_DIR)
	lcov --capture --directory . --output-file $(COVERAGE_DIR)/lcov.info \
	     --include "*/Mgmt/src/*" --branch-coverage
	genhtml $(COVERAGE_DIR)/lcov.info --output-directory $(COVERAGE_DIR)/html \
	     --branch-coverage
	@echo "Report: $(COVERAGE_DIR)/html/index.html"

clean:
	rm -f $(TESTS)
	rm -f *.gcda *.gcno mgmt/*.gcda mgmt/*.gcno
	rm -rf $(COVERAGE_DIR)
