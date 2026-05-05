#!/bin/bash
# tmux セッション残存確認スクリプト

if tmux has-session -t studyC 2>/dev/null; then
    echo "[残存あり] studyC セッションが残っています"
    tmux list-panes -t studyC -F "  pane #{pane_index}: #{pane_current_command}"
    echo ""
    echo "終了するには: tmux kill-session -t studyC"
else
    echo "[クリーン] studyC セッションはありません"
fi
