# env.mkが存在するかチェックし、なければテンプレートから自動生成する
ifeq ("$(wildcard env.mk)","")
$(info [INFO] env.mk not found. Copying from env.mk.template...)
$(shell cp env.mk.template env.mk)
endif

# その上で環境変数を読み込む
include env.mk

SUBDIRS = Common SHM Mgmt TestMsgRcv PollIO TestUDPKill

.PHONY: all clean $(SUBDIRS)

all: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@

clean:
	@for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir clean; \
	done