#!/bin/bash

#make clean-all
#bear -- make
#sh run_clang_tidy.sh
set -u

# clang-tidyがインストールされているか確認
if ! command -v clang-tidy &> /dev/null; then
    echo "Error: clang-tidy is not installed."
    echo "Please install it (e.g., sudo apt install clang-tidy)."
    exit 1
fi

PROJECT_ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$PROJECT_ROOT" || exit 1

# チェック対象のモジュール
MODULES=("Common" "PosixSHM" "Mgmt" "Collector" "Router" "Viewer" "MgmtCtl")

# compile_commands.json を使うかどうかの判定
if [ -f "compile_commands.json" ]; then
    echo "[Info] Using compile_commands.json for accurate analysis."
    USE_COMPDB=1
else
    echo "[Info] compile_commands.json not found. Using fallback manual build flags."
    USE_COMPDB=0
    
    # プロジェクト内の各 src/ をインクルードパスに追加
    INCLUDES=""
    for mod in "${MODULES[@]}"; do
        if [ -d "$mod/src" ]; then
            INCLUDES="$INCLUDES -I$PROJECT_ROOT/$mod/src"
        fi
    done
    
    # README記載のビルドフラグに合わせる
    CFLAGS="-std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra"
fi

# 適用するチェックルール (C言語向けに有用なものを中心に設定)
# ※ プロジェクトの要件に合わせて適宜変更してください
CHECKS="bugprone-*,cert-*,clang-analyzer-*,performance-*,portability-*,readability-*,-readability-identifier-length,-readability-magic-numbers,-clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling,-bugprone-reserved-identifier,-cert-err33-c,-readability-braces-around-statements,-cert-dcl37-c,-cert-dcl51-cpp,-bugprone-easily-swappable-parameters,-readability-function-cognitive-complexity,-readability-isolate-declaration,-readability-else-after-return"

echo "Starting clang-tidy checks..."

for mod in "${MODULES[@]}"; do
    if [ ! -d "$mod/src" ]; then
        continue
    fi
    
    echo ""
    echo "========================================"
    echo " Checking module: $mod"
    echo "========================================"
    
    # .c ファイルを取得してループ処理
    while IFS= read -r -d '' file; do
        echo "-> $file"
        
        if [ "$USE_COMPDB" -eq 1 ]; then
            clang-tidy -p . -checks="$CHECKS" "$file"
        else
            clang-tidy -checks="$CHECKS" "$file" -- $CFLAGS $INCLUDES
        fi
        
    done < <(find "$mod/src" -name "*.c" -print0)
done
