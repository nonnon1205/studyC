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

# チェック内容は .clang-tidy で管理する
echo "Starting clang-tidy checks..."

for mod in "${MODULES[@]}"; do
    if [ ! -d "$mod/src" ]; then
        continue
    fi

    echo ""
    echo "========================================"
    echo " Checking module: $mod"
    echo "========================================"

    while IFS= read -r -d '' file; do
        echo "-> $file"

        if [ "$USE_COMPDB" -eq 1 ]; then
            clang-tidy -p . "$file"
        else
            clang-tidy "$file" -- $CFLAGS $INCLUDES
        fi

    done < <(find "$mod/src" -name "*.c" -print0)
done
