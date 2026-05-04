# 既知の問題一覧

CI が整うまで修正を保留している問題を記録する。
修正着手時はここから削除する。

---

## EdgeCase_TestCases.md のバッファサイズが実装と不一致
- **場所**: [docs/EdgeCase_TestCases.md](EdgeCase_TestCases.md) — 項目 1-2, 1-3
- **内容**: テスト基準値が 1023/1024 バイトだが、SHM の `message` フィールドは `char message[256]`。UDP 受信バッファ（1024）と SHM バッファ（256）を混同している。
- **優先度**: high

## Router/main.c の ENABLE_FAULT_INJECTION ブロックに構文エラー
- **場所**: [Router/src/main.c:347](../Router/src/main.c#L347)
- **内容**: `ENABLE_FAULT_INJECTION` 有効時に `}` が余分。cppcheck が `syntaxError` を報告する。
- **優先度**: medium

## mgmt_handlers.c の constVariablePointer
- **場所**: [Router/src/mgmt_handlers.c:29, 42](../Router/src/mgmt_handlers.c#L29)
- **内容**: `RouterMgmtCtx* c` は `const RouterMgmtCtx*` にできる。cppcheck の style 指摘。
- **優先度**: low
