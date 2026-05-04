# CI構築およびリファクタリング推進計画

## 目的
システムの境界値バグ（SIGPIPEクラッシュ等）やコードの粗（マジックナンバー散在等）を修正するにあたり、修正によるデグレを防ぐための自動テスト（CI）環境を先行して構築する。
テスト駆動開発（TDD）のサイクルを回すことで、安全かつ確実なシステム堅牢化を実現する。

---

## フェーズと手順

### Phase 1: 自動テストの土台作り (E2Eテスト) — **一部完了**

| 作業 | 状態 |
|---|---|
| E2E テストスクリプト作成 (`conftest.py`, `test_data_pipeline.py`) | 完了 |
| `tests/e2e/` へのディレクトリ配置 | 完了 |
| `tests/requirements.txt` の配置 | 完了 |
| Makefile に `make test` ターゲット追加 | **未着手** |
| E2E テストの実行検証 | **未着手** |

**残作業:**
```makefile
# Makefile に追加する
test:
	pip install -q -r tests/requirements.txt
	cd tests && pytest e2e/ -v
```

---

### Phase 2: テスト駆動でのクリティカルな粗の修正 — **未着手**

1. **ネットワーク異常系対策**: Router プロセスでの TCP 送信時における `SIGPIPE` クラッシュ対策（`MSG_NOSIGNAL` 等の適用）
2. **マジックナンバーの排除**: `7777`, `8888`, バッファサイズ等を `msg_common.h` などの共通ヘッダに集約

---

### Phase 3: CIパイプラインの自動化 — **静的解析のみ完了**

| 作業 | 状態 |
|---|---|
| 静的解析 cppcheck 導入 | 完了 |
| 静的解析 clang-tidy 導入 (`run_clang_tidy.sh` + `.clang-tidy`) | 完了 |
| CLAUDE.md への静的解析実行ルール追記 | 完了 |
| 静的解析で発見したバグ修正（`ssize_t`, `snprintf`, `safe_strerror` 等） | 完了 |
| GitHub Actions ワークフローファイル作成 | **未着手** |
| push 時の自動パイプライン（ビルド→静的解析→E2E）構築 | **未着手** |

**GitHub Actions 構成イメージ（未着手）:**
```yaml
# .github/workflows/ci.yml
jobs:
  build-and-test:
    steps:
      - run: make all && make debug
      - run: cppcheck --enable=warning,style --std=c11 ...
      - run: bash run_clang_tidy.sh
      - run: make test   # Phase 1 完了後に有効化
```

---

### Phase 4: その他の高度な改善（次期課題）

| 作業 | 状態 |
|---|---|
| エッジケーステスト仕様策定 (`EdgeCase_TestCases.md`) | 完了 |
| エッジケーステストの実装 | **未着手** |
| MgmtCtl 統合（GET_METRICS / RESET_METRICS ハンドラ） | **未着手** |
| リソースリーク検査（Valgrind 導入） | **未着手** |
| テストカバレッジ計測（gcov / lcov） | **未着手** |

---

## 運用ルール
今後は「コードを修正する前に、その挙動を保証（またはバグを再現）するテストが存在するか」を意識し、テストと実装をセットで進めること。

## 次に着手すべき作業
1. Makefile に `make test` ターゲットを追加し E2E テストを実行できる状態にする（Phase 1 完結）
2. E2E テストが通ることを確認してから Phase 2（SIGPIPE 対策）に入る
