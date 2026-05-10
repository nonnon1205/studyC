# テスト戦略・環境構成仕様書

## 1. テストレベルと方針

本プロジェクトのテストはピラミッドを逆順に構築した経緯がある。現在の位置づけは以下のとおり。

| レベル | 目的 | ツール | 状態 |
|---|---|---|---|
| E2E（結合） | プロセス間通信の貫通確認 | Python / pytest | 完了 |
| 単体（Unity） | C 関数単位のロジック検証 | Unity / gcov | 完了 |
| 単体（Google Test） | C++ フレームワークの評価・比較 | Google Test / Google Mock | 完了 |
| 単体（CppTest） | フレームワーク比較・学習 | CppTest | 学習完了 |
| モック生成（CMock） | 自動モック生成の評価・学習 | CMock / Ruby | 学習完了 |

---

## 2. テスト環境構成（JSTQB 5.4 構成管理）

テスト結果の再現性を保証するため、環境を構成管理の対象とする。

### 2.1 必要ツールとバージョン

| ツール | バージョン | バージョン確認コマンド | 用途 |
|---|---|---|---|
| gcc | 13.3.0 | `gcc --version` | C テストバイナリのビルド |
| g++ | 13.3.0 | `g++ --version` | Google Test / CppTest テストバイナリのビルド |
| Unity | 手動管理（`tests/unity/` 参照） | — | C 単体テストフレームワーク |
| Google Test | 1.14.0 | `pkg-config --modversion gtest` | C++ 単体テストフレームワーク |
| Google Mock | 1.14.0 | `pkg-config --modversion gmock` | C++ モックフレームワーク |
| CppTest | 2.0.0 | `dpkg -l libcpptest-dev` | C++ 単体テストフレームワーク（比較用） |
| CMock | 2.5.3 | `dpkg -l cmock` | C モック自動生成（学習用） |
| Ruby | 3.2 | `ruby --version` | CMock スクリプト実行 |
| lcov | 2.0-1 | `lcov --version` | カバレッジ HTML レポート生成 |
| gcov | 13.3.0 | `gcov --version` | gcc 同梱カバレッジツール |
| pytest | — | `python3 -m pytest --version` | E2E テストランナー |

### 2.2 インストール手順

```bash
# Google Test / Google Mock
sudo apt install libgtest-dev libgmock-dev

# CppTest
sudo apt install libcpptest-dev

# CMock / Ruby
sudo apt install cmock ruby

# lcov
sudo apt install lcov

# Python 依存
python3 -m pip install -r tests/requirements.txt
```

### 2.3 ビルドフラグ

| ツール | 用途 | フラグ |
|---|---|---|
| gcc | 通常テストビルド（Unity） | `-std=c17 -Wall -Wextra` |
| gcc | カバレッジビルド | 上記 + `--coverage -O0` |
| gcc | ASan ビルド | 上記 + `-fsanitize=address,undefined -g` |
| g++ | 通常テストビルド（Google Test / CppTest） | `-std=c++17 -Wall -Wextra` |
| g++ | Google Test リンク | 上記 + `-lgtest -lgtest_main -pthread` |
| g++ | Google Mock リンク | 上記 + `-lgmock` |
| g++ | CppTest リンク | 上記 + `-lcpptest` |

### 2.4 --wrap リンカオプション（C 関数のモック）

システムコールを差し替えてテストする際に使用する。プロダクションコードを変更せずにモックを注入できる。

```bash
-Wl,--wrap=socket,--wrap=bind,--wrap=sendto,--wrap=poll,--wrap=recv,--wrap=close,--wrap=unlink
```

`__wrap_xxx()` を定義すると `xxx()` への呼び出しが全てリダイレクトされる。`__real_xxx()` で本物を呼ぶことも可能。

### 2.5 Google Test のリンク構成

`libgtest-dev` は apt でソースのみインストールされる。`-lgtest` でリンクするために
`/usr/lib` に `.a` が存在することを確認する。

```bash
ls /usr/lib/x86_64-linux-gnu/libgtest*.a
# → libgtest.a と libgtest_main.a が存在すること
```

---

## 3. ディレクトリ構成

```text
studyC/
├── tests/
│   ├── unity/                            # Unity フレームワーク本体（手動管理）
│   │   ├── unity.c
│   │   ├── unity.h
│   │   └── unity_internals.h
│   ├── unit/                             # 単体テスト
│   │   ├── Makefile
│   │   ├── unity/mgmt/                   # Unity テスト
│   │   │   ├── test_mgmt_protocol.c          # mgmt_protocol.c テスト（52件）
│   │   │   ├── test_mgmt_send_unity.c        # mgmt_send.c テスト（stub 方式）
│   │   │   ├── stub_syscalls.c               # --wrap スタブ実装
│   │   │   └── stub_syscalls.h
│   │   ├── gtest/mgmt/                   # Google Test / Google Mock テスト
│   │   │   ├── test_mgmt_protocol_gtest.cpp      # mgmt_protocol.c テスト（23件）
│   │   │   ├── test_mgmt_send_real_gmock.cpp     # mgmt_send.c テスト（GMock + --wrap）
│   │   │   ├── test_mgmt_advanced_gtest.cpp      # TEST_P / Death テストデモ
│   │   │   ├── test_mgmt_send_gmock.cpp          # GMock C++ ラッパー方式（学習用）
│   │   │   ├── ISocketOps.h                      # C++ インタフェース（学習用）
│   │   │   └── MgmtSender.h                      # C++ ラッパー（学習用）
│   │   └── cpptest/mgmt/                 # CppTest テスト（比較・学習用）
│   │       └── test_mgmt_protocol_cpptest.cpp
│   └── e2e/                              # E2E テスト（Python / pytest）
│       ├── conftest.py
│       └── test_*.py
│
└── docs/test/
    ├── Test_Strategy.md                  # 本ファイル
    ├── UT_mgmt_protocol.md               # 単体テスト項目表（Unity 基準）
    └── TestSpec_E2E.md                   # E2E テスト仕様
```

---

## 4. テストケースの追跡可能性

| 成果物 | 場所 | 内容 |
|---|---|---|
| UT 項目表 | `docs/test/UT_mgmt_protocol.md` | 関数・入力・期待値・種別の対応（No.1〜52） |
| テストコード（Unity） | `tests/unit/unity/mgmt/test_mgmt_protocol.c` | 項目表 No. と 1:1 対応 |
| テストコード（Unity stub） | `tests/unit/unity/mgmt/test_mgmt_send_unity.c` | --wrap スタブによる mgmt_send.c 検証 |
| テストコード（Google Test） | `tests/unit/gtest/mgmt/test_mgmt_protocol_gtest.cpp` | 同上関数を C++ で記述・比較用 |
| テストコード（Google Mock） | `tests/unit/gtest/mgmt/test_mgmt_send_real_gmock.cpp` | --wrap + GMock による mgmt_send.c 検証 |
| テストコード（CppTest） | `tests/unit/cpptest/mgmt/test_mgmt_protocol_cpptest.cpp` | フレームワーク比較用 |
| カバレッジレポート | `tests/unit/coverage/html/` | lcov 生成・gitignore 対象 |

---

## 5. テスト実行コマンド

```bash
# 単体テスト（Unity）
make -C tests/unit

# カバレッジ計測（行・関数・ブランチ）
make -C tests/unit coverage

# 単体テスト（Google Test）
make -C tests/unit gtest

# 単体テスト（Google Mock + --wrap）
make -C tests/unit gmock-real

# 単体テスト（Unity stub + --wrap）
make -C tests/unit unity-send

# E2E テスト
make test

# ASan 付き E2E テスト
make test-asan
```

---

## 6. フレームワーク比較

| 観点 | Unity | Google Test | Google Mock | CppTest | CMock |
|---|---|---|---|---|---|
| 対象言語 | C | C++ | C++ | C++ | C |
| モック | 手書きスタブ | — | 自動生成 | — | 自動生成 |
| システムコール差し替え | `--wrap` + 手書き | `--wrap` + Mock クラス | 同左 | — | 抽象レイヤー必要 |
| カバレッジ | gcov 対応 | gcov 対応 | — | — | — |
| 適用場面 | C コード単体テスト | C++ / 比較検証 | モック検証 | 学習・比較 | 抽象レイヤー設計済みの現場 |

---

## 7. カバレッジ目標

| 指標 | 目標 | 現状（mgmt_protocol.c） |
|---|---|---|
| 行カバレッジ | 100% | 100% |
| 関数カバレッジ | 100% | 100% |
| ブランチカバレッジ | 100% | 100% |

---

## 8. 除外対象

以下はカバレッジ・レビュー対象外とする。

- `tests/unity/` — フレームワーク本体
- `tests/e2e/__pycache__/` — Python キャッシュ
- `tests/unit/coverage/` — 生成物（gitignore 済み）
- `tests/unit/unity/mgmt/cmock_demo/` — CMock 学習用デモ（プロダクション対象外）
