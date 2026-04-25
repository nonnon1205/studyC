# --- Machine Dependent Paths ---
# カーネルヘッダ等が必要な場合はここを書き換える
KERNEL_DIR ?= /lib/modules/$(shell uname -r)/build
SDK_ROOT   ?= 

# --- Tools ---
CC         ?= gcc
AR         ?= ar
INSTALL    ?= install

# --- Build Options ---
DEBUG      ?= -g
OPT        ?= -O0
CFLAGS     ?= -Wall -Wextra $(DEBUG) $(OPT)