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

.PHONY: all debug clean $(SUBDIRS)

all: $(SUBDIRS)

debug:
	$(MAKE) SHM_IMPL=$(SHM_IMPL) IFDEF="-DDEBUG" all

$(SUBDIRS):
	$(MAKE) -C $@ IFDEF=$(IFDEF)

clean:
	@for dir in $(CLEAN_DIRS); do \
		$(MAKE) -C $$dir clean; \
	done