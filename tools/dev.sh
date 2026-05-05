#!/bin/bash
# 開発環境起動スクリプト
# 使い方: ./tools/dev.sh
# デタッチ: Ctrl+b → d
# 終了:    tmux kill-session -t studyC
#
# レイアウト (縦並び):
#   Viewer   (大)
#   Router   (小)
#   Collector(小)
#   mgmtctl  (中)

SESSION="studyC"
ROOT=$(cd "$(dirname "$0")/.." && pwd)

if tmux has-session -t "$SESSION" 2>/dev/null; then
    echo "[dev.sh] セッション '$SESSION' に接続します..."
    tmux attach-session -t "$SESSION"
    exit 0
fi

COLS=$(tput cols)
LINES=$(tput lines)

# 1. セッション作成 → Viewer
PANE_VIEWER=$(tmux new-session -d -s "$SESSION" -c "$ROOT" \
    -x "$COLS" -y "$LINES" -P -F '#{pane_id}')

# 2. Viewer(40%) / 残り(60%)
PANE_R1=$(tmux split-window -v -l 60% -t "$PANE_VIEWER" -P -F '#{pane_id}')

# 3. 残り(60%) → Router(小) / 残り
PANE_R2=$(tmux split-window -v -l 67% -t "$PANE_R1" -P -F '#{pane_id}')

# 4. 残り → Collector(小) / mgmtctl
PANE_MGMTCTL=$(tmux split-window -v -l 50% -t "$PANE_R2" -P -F '#{pane_id}')
PANE_COLLECTOR=$PANE_R2
PANE_ROUTER=$PANE_R1

# 5. 各ペインにコマンドを送信
tmux send-keys -t "$PANE_VIEWER"    "$ROOT/build/Viewer" Enter
tmux send-keys -t "$PANE_ROUTER"    "$ROOT/build/Router" Enter
tmux send-keys -t "$PANE_COLLECTOR" "$ROOT/build/Collector" Enter
tmux send-keys -t "$PANE_MGMTCTL"  "cd '$ROOT'" Enter

# 6. mgmtctl にフォーカスしてアタッチ
tmux select-pane -t "$PANE_MGMTCTL"
tmux attach-session -t "$SESSION"
