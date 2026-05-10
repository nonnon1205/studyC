ROOT         = ../..
UNITY        = $(ROOT)/tests/unity
CC           = gcc
CXX          = g++
CFLAGS       = -std=c17 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -I$(UNITY) -I$(ROOT)/Mgmt/include
CXXFLAGS     = -std=c++17 -Wall -Wextra -I$(ROOT)/Mgmt/include

TESTS        = unity/mgmt/test_mgmt_protocol
GTEST        = gtest/mgmt/test_mgmt_protocol_gtest
GMOCK        = gtest/mgmt/test_mgmt_send_gmock
GMOCK_REAL   = gtest/mgmt/test_mgmt_send_real_gmock
COVERAGE_DIR = coverage

.PHONY: all clean coverage gtest gmock gmock-real unity-send

all: $(TESTS)
	@echo ""
	@for t in $(TESTS); do ./$$t; echo ""; done

unity/mgmt/test_mgmt_protocol: unity/mgmt/test_mgmt_protocol.c \
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

gtest: $(GTEST)
	./$(GTEST)

gmock: $(GMOCK)
	./$(GMOCK)

gtest/mgmt/test_mgmt_send_gmock: gtest/mgmt/test_mgmt_send_gmock.cpp \
                                  $(ROOT)/Mgmt/src/mgmt_protocol.c
	$(CC) $(CFLAGS) -c -o gtest/mgmt/mgmt_protocol_gmock.o $(ROOT)/Mgmt/src/mgmt_protocol.c
	$(CXX) $(CXXFLAGS) -Igtest/mgmt -o $@ gtest/mgmt/test_mgmt_send_gmock.cpp \
	    gtest/mgmt/mgmt_protocol_gmock.o -lgtest -lgtest_main -lgmock -pthread
	rm -f gtest/mgmt/mgmt_protocol_gmock.o

gtest/mgmt/test_mgmt_protocol_gtest: gtest/mgmt/test_mgmt_protocol_gtest.cpp \
                                      $(ROOT)/Mgmt/src/mgmt_protocol.c
	$(CC) $(CFLAGS) -c -o gtest/mgmt/mgmt_protocol_gtest.o $(ROOT)/Mgmt/src/mgmt_protocol.c
	$(CXX) $(CXXFLAGS) -o $@ gtest/mgmt/test_mgmt_protocol_gtest.cpp \
	    gtest/mgmt/mgmt_protocol_gtest.o -lgtest -lgtest_main -pthread
	rm -f gtest/mgmt/mgmt_protocol_gtest.o

gmock-real: $(GMOCK_REAL)
	./$(GMOCK_REAL)

gtest/mgmt/test_mgmt_send_real_gmock: gtest/mgmt/test_mgmt_send_real_gmock.cpp \
                                       $(ROOT)/MgmtCtl/src/mgmt_send.c
	$(CC) $(CFLAGS) -I$(ROOT)/MgmtCtl/include -c -o gtest/mgmt/mgmt_send_real.o \
	    $(ROOT)/MgmtCtl/src/mgmt_send.c
	$(CXX) $(CXXFLAGS) -I$(ROOT)/MgmtCtl/include -Igtest/mgmt -o $@ \
	    gtest/mgmt/test_mgmt_send_real_gmock.cpp gtest/mgmt/mgmt_send_real.o \
	    -lgtest -lgtest_main -lgmock -pthread \
	    -Wl,--wrap=socket,--wrap=bind,--wrap=sendto,--wrap=poll,--wrap=recv,--wrap=close,--wrap=unlink
	rm -f gtest/mgmt/mgmt_send_real.o

unity-send: unity/mgmt/test_mgmt_send_unity
	./unity/mgmt/test_mgmt_send_unity

unity/mgmt/test_mgmt_send_unity: unity/mgmt/test_mgmt_send_unity.c \
                                  unity/mgmt/stub_syscalls.c \
                                  $(ROOT)/MgmtCtl/src/mgmt_send.c \
                                  $(UNITY)/unity.c
	$(CC) $(CFLAGS) -I$(ROOT)/MgmtCtl/include -Iunity/mgmt -o $@ $^ \
	    -Wl,--wrap=socket,--wrap=bind,--wrap=sendto,--wrap=poll,--wrap=recv,--wrap=close,--wrap=unlink

clean:
	rm -f $(TESTS) $(GTEST) $(GMOCK) $(GMOCK_REAL)
	rm -f unity/mgmt/test_mgmt_send_unity
	rm -f *.gcda *.gcno unity/mgmt/*.gcda unity/mgmt/*.gcno
	rm -rf $(COVERAGE_DIR)
