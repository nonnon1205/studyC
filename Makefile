# env.mkが存在するかチェックし、なければテンプレートから自動生成する
ifeq ("$(wildcard env.mk)","")
$(info [INFO] env.mk not found. Copying from env.mk.template...)
$(shell cp env.mk.template env.mk)
endif

# その上で環境変数を読み込む
include env.mk

# SHM_IMPL=sysv (default) または SHM_IMPL=posix でビルド時に切り替える
SHM_IMPL ?= sysv

ifeq ($(SHM_IMPL),posix)
SHM_MODULE = PosixSHM
else
SHM_MODULE = SHM
endif

SUBDIRS      = Common $(SHM_MODULE) Mgmt Collector Router Viewer MgmtCtl
CLEAN_DIRS   = Common SHM PosixSHM Mgmt Collector Router Viewer MgmtCtl

.PHONY: all debug asan tsan clean lint test test-asan test-tsan asan-fault test-fault-uaf test-fault-leak $(SUBDIRS)

all: $(SUBDIRS)

debug:
	$(MAKE) SHM_IMPL=$(SHM_IMPL) IFDEF="-DDEBUG" all

asan: clean
	$(MAKE) SHM_IMPL=$(SHM_IMPL) IFDEF="-DDEBUG" CC="gcc -fsanitize=address,undefined -g -fno-omit-frame-pointer" all

asan-fault: clean
	$(MAKE) SHM_IMPL=$(SHM_IMPL) IFDEF="-DDEBUG -DENABLE_FAULT_INJECTION" CC="gcc -fsanitize=address,undefined -g -fno-omit-frame-pointer" all

tsan: clean
	$(MAKE) SHM_IMPL=$(SHM_IMPL) IFDEF="-DDEBUG" CC="gcc -fsanitize=thread -g -fno-omit-frame-pointer" all

$(SUBDIRS):
	$(MAKE) -C $@ IFDEF="$(IFDEF)"

lint:
	cppcheck --enable=warning,style --std=c17 -ICommon/include -ISHM/include -IMgmt/include Collector/src Router/src Viewer/src MgmtCtl/src
	bash run_clang_tidy.sh

test: all
	python3 -m pip install -q -r tests/requirements.txt
	cd tests && python3 -m pytest e2e/ -v

test-asan: asan
	python3 -m pip install -q -r tests/requirements.txt
	cd tests && ASAN_OPTIONS=detect_leaks=1 python3 -m pytest e2e/ -v

test-tsan: tsan
	python3 -m pip install -q -r tests/requirements.txt
	cd tests && TSAN_OPTIONS="history_size=7" python3 -m pytest e2e/ -v

# ASanのデモ用ターゲット
test-fault-uaf: asan-fault
	@echo "\n\n💥 AddressSanitizer: Use-After-Free のエラー出力をデモします 💥\n"
	@./build/Router --fail-use-after-free || true

test-fault-leak: asan-fault
	@echo "\n\n💧 AddressSanitizer: メモリリークのエラー出力をデモします 💧\n"
	@ASAN_OPTIONS=detect_leaks=1 ./build/Router --fail-leak || true

clean:
	@for dir in $(CLEAN_DIRS); do \
		$(MAKE) -C $$dir clean; \
	done
	rm -rf build/