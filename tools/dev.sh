#!/bin/bash
# 開発環境起動スクリプト
# 使い方: ./dev.sh
# デタッチ: Ctrl+b → d
# 終了:    tmux kill-session -t studyC

SESSION="studyC"
ROOT=$(cd "$(dirname "$0")/.." && pwd)

if tmux has-session -t "$SESSION" 2>/dev/null; then
    echo "[dev.sh] セッション '$SESSION' に接続します..."
    tmux attach-session -t "$SESSION"
    exit 0
fi

# ペイン ID を変数に保持しながら分割する
# pane_top_left  → syslog
# pane_top_right → Viewer
# pane_bot_left  → Collector
# pane_bot_mid   → Router
# pane_bot_right → mgmtctl

# 1. セッション作成（最初のペイン = syslog）
COLS=$(tput cols)
LINES=$(tput lines)
PANE_SYSLOG=$(tmux new-session -d -s "$SESSION" -c "$ROOT" -x "$COLS" -y "$LINES" -P -F '#{pane_id}')

# 2. 上段 / 下段 に分割（下35%）
PANE_BOT=$(tmux split-window -v -l 35% -t "$PANE_SYSLOG" -P -F '#{pane_id}')

# 3. 上段を左右に分割: syslog(55%) | Viewer(45%)
PANE_VIEWER=$(tmux split-window -h -l 45% -t "$PANE_SYSLOG" -P -F '#{pane_id}')

# 4. 下段を 3 分割: Collector(20%) | Router(20%) | mgmtctl(残り)
PANE_BOT_R=$(tmux split-window -h -l 80% -t "$PANE_BOT" -P -F '#{pane_id}')
PANE_MGMTCTL=$(tmux split-window -h -l 75% -t "$PANE_BOT_R" -P -F '#{pane_id}')
PANE_ROUTER=$PANE_BOT_R

# 5. 各ペインにコマンドを送信
tmux send-keys -t "$PANE_SYSLOG"   "tail -f /var/log/syslog" Enter
tmux send-keys -t "$PANE_VIEWER"   "$ROOT/build/Viewer" Enter
tmux send-keys -t "$PANE_BOT"      "$ROOT/build/Collector" Enter
tmux send-keys -t "$PANE_ROUTER"   "$ROOT/build/Router" Enter
tmux send-keys -t "$PANE_MGMTCTL"  "cd '$ROOT'" Enter

# 6. mgmtctl ペインにフォーカスしてアタッチ
tmux select-pane -t "$PANE_MGMTCTL"
tmux attach-session -t "$SESSION"
