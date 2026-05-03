# 共通環境の読み込み
-include ../env.mk

# モジュール固有のコンパイルオプション
override CFLAGS += -pthread -Isrc -I../lib/include $(IFDEF)

# パス設定
PREFIX  := $(abspath ..)
LIBDIR  := $(PREFIX)/lib
INCDIR  := $(LIBDIR)/include

# ターゲット
TARGET  := $(LIBDIR)/libshm.a
SRCS    := $(wildcard src/*.c)
OBJS    := $(SRCS:src/%.c=obj/%.o)
HDRS    := $(wildcard include/*.h)

.PHONY: all clean install

all: $(TARGET) install

# 1. ライブラリの生成
$(TARGET): $(OBJS)
	@mkdir -p $(LIBDIR)
	$(AR) rcs $@ $^

# 2. オブジェクトファイルのビルド
obj/%.o: src/%.c | obj
	$(CC) $(CFLAGS) -c $< -o $@

obj:
	mkdir -p obj

# 3. インストール（ヘッダのコピー）
install: $(HDRS)
	mkdir -p $(INCDIR)
	$(if $(HDRS),$(INSTALL) -m 644 $^ $(INCDIR)/)

clean:
	rm -rf obj $(TARGET) $(if $(HDRS),$(patsubst include/%.h,$(INCDIR)/%.h,$(HDRS)))
