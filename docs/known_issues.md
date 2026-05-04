# 既知の問題一覧

CI が整うまで修正を保留している問題を記録する。
修正着手時はここから削除する。

---

## mgmt_handlers.c の constVariablePointer
- **場所**: [Router/src/mgmt_handlers.c:29, 42](../Router/src/mgmt_handlers.c#L29)
- **内容**: `RouterMgmtCtx* c` は `const RouterMgmtCtx*` にできる。cppcheck の style 指摘。
- **優先度**: low
