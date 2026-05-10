# studyC

C言語システムプログラミングの学習プロジェクト。
マルチプロセス・マルチスレッドのデータパイプラインと、実行中プロセスへの管理コマンド基盤を実装する。

---

## 学習完了状態（2026-05-10）

このプロジェクトは学習目的を達成した時点で完了とした。追加実装は「積み残し」として記録しているが、積極的に進める予定はない。

### 完了フェーズ

| フェーズ | 内容 |
|---|---|
| 実装 | Collector / Router / Viewer / SHM / Common / Mgmt / MgmtCtl |
| ロギング | GLOG_* 2層構造（syslog + DBG）、全モジュール移行 |
| Mgmt 統合 | Phase 0–5（ライブラリ化・ハンドラ登録・プロトコル・各モジュール統合） |
| 単体テスト（Unity） | mgmt_protocol.c 52件、mgmt_send.c 5件、ブランチカバレッジ 100% |
| 単体テスト（GoogleTest / GMock） | mgmt_protocol.c 23件、mgmt_send.c 5件、TEST_P / Death テスト |
| フレームワーク比較 | CppTest・CMock の評価・学習デモ |
| CI 組み込み | GitHub Actions（ビルド・静的解析・単体テスト・E2E テスト） |

### 積み残し（対応予定なし）

- `GET_METRICS` / `RESET_METRICS` ハンドラ（MetricsHandle の組み込みが必要）
- `mgmtctl all shutdown`（全プロセス一括停止）
- `docs/api/APISpecification.text` の旧モジュール名更新
- ファイル命名規則の整理（例: `udp_worker.c` → `router_udp_worker.c`）

---

## アーキテクチャ

```
[外部送信者]
     |
     | UDP :9999
     v
+------------+     SHM + MQ      +------------+     TCP :7777     +------------+
| Collector  | ----------------> |   Router   | ----------------> |   Viewer   |
| (1 thread) |   IPC key:0x54321 | (3 threads)|                   | (4 threads)|
+-----+------+                   +-----+------+                   +-----+------+
      |                                |                                 |
  UNIX sock                        UNIX sock                         UNIX sock
  collector.sock                   router.sock                       viewer.sock
      ^                                ^                                 ^
      +--------------------------------+---------------------------------+
                                       |
                                  [ mgmtctl ]
                               (コマンドライン CLI)
```

### データフロー

1. **Collector** — UDP ポート（デフォルト `9999`）でデータを受信し、共有メモリ (SHM) に書き込む。System V メッセージキュー (IPC key `0x54321`) で Router に通知する。
2. **Router** — MQ 通知を受け取り、SHM からペイロードを読み出して TCP ポート（デフォルト `7777`）へ中継する。
3. **Viewer** — TCP ポート（デフォルト `7777`）を Listen し、受信データをコンソールに表示する。
*(※ 各ポート番号は環境変数で上書き可能です)*

### 管理インターフェース

各プロセスは UNIX ドメインソケットを持ち、実行中に外部からコマンドを受け付ける。

| プロセス | ソケットパス |
|---------|-------------|
| Collector | `/tmp/studyc_collector.sock` |
| Router | `/tmp/studyc_router.sock` |
| Viewer | `/tmp/studyc_viewer.sock` |

---

## モジュール構成

| ディレクトリ | 種別 | 役割 |
|-------------|------|------|
| `Common/` | ライブラリ | 統合ロガー → `libcommon.a` |
| `SHM/` | ライブラリ | 共有メモリ API → `libshm.a` |
| `Mgmt/` | ライブラリ | 管理ソケット・ハンドラ登録・プロトコル → `libmgmt.a` |
| `Collector/` | プロセス | UDP受信 → SHM書込 + MQ通知 |
| `Router/` | プロセス | MQ受信 → SHM読出 → TCP中継 |
| `Viewer/` | プロセス | TCP受信 → コンソール表示 |
| `MgmtCtl/` | CLI | 各プロセスへ管理コマンドを送る `mgmtctl` バイナリ |

ビルド成果物のライブラリは `lib/` に出力される。

---

## ビルド

### 前提

- GCC (`gcc`)
- GNU Make
- pthreads（glibc 付属）

### 初回セットアップ

```bash
# env.mk が存在しない場合は make が自動生成するが、
# カスタマイズが必要なときはテンプレートを手動コピーして編集する
cp env.mk.template env.mk
```

### フルビルド

```bash
make
```

内部のビルド順序: `Common` → `SHM` → `Mgmt` → `Collector` / `Router` / `Viewer` / `MgmtCtl`

### デバッグビルド

```bash
make debug   # -DDEBUG フラグ付きでビルド
```

### クリーン

```bash
make clean
```

---
### 単体テスト

```bash
# Unity（C）
make -C tests/unit          # mgmt_protocol.c テスト
make -C tests/unit unity-send   # mgmt_send.c テスト（--wrap スタブ）

# GoogleTest / GMock（C++）
make -C tests/unit gtest        # mgmt_protocol.c テスト
make -C tests/unit gmock-real   # mgmt_send.c テスト（--wrap + GMock）
```

詳細は [docs/test/Test_Strategy.md](docs/test/Test_Strategy.md) を参照。

### E2E テスト (CI)

```bash
make test
```

Python (`pytest`) を用いた E2E テストが実行される。
GitHub Actions により Push 時にビルド・静的解析（`cppcheck`, `clang-tidy`）・単体テスト・E2E テストが自動実行される。


## 起動と動作確認

### 1. 各プロセスを起動する（ターミナル 3 枚）

```bash
# Terminal A
./Collector/Collector

# Terminal B
./Router/Router

# Terminal C
./Viewer/Viewer
```

起動順序は任意。各プロセスは接続相手が現れるまで待機する。

### 2. テストデータを送信する

デフォルトの UDP ポート 9999 に任意のデータを送ると、Collector → Router → Viewer と転送され、Viewer のコンソールに表示されます。

```bash
# nc を使う例
echo "hello" | nc -u 127.0.0.1 9999
```

### 3. 管理コマンドを送る

```bash
./MgmtCtl/mgmtctl <module> <command> [payload]
```

| コマンド例 | 説明 |
|-----------|------|
| `mgmtctl collector ping` | 死活確認（"pong" が返る） |
| `mgmtctl router get_status` | 稼働時間などのステータス JSON |
| `mgmtctl viewer get_status` | 稼働時間などのステータス JSON |
| `mgmtctl collector shutdown` | Collector をクリーンに停止 |
| `mgmtctl router shutdown` | Router をクリーンに停止 |
| `mgmtctl viewer shutdown` | Viewer をクリーンに停止 |

### 4. プロセスを停止する

```bash
# Ctrl-C または mgmtctl shutdown
./MgmtCtl/mgmtctl collector shutdown
./MgmtCtl/mgmtctl router shutdown
./MgmtCtl/mgmtctl viewer shutdown
```

---

## ロギング

`syslog` に出力される。`journalctl` または `/var/log/syslog` で確認できる。

```bash
journalctl -f -t Collector -t Router -t Viewer
```

---

## 開発メモ

詳細な設計・規約は [CLAUDE.md](CLAUDE.md) を参照。

- ビルドフラグ: `-std=c17 -D_POSIX_C_SOURCE=200809L -Wall -Wextra`
- ロガーマクロ: `GLOG_INFO / GLOG_ERR / GLOG_FATAL`（`<syslog.h>` の `LOG_*` との衝突を避けるため `GLOG_` プレフィックス）
- 初期化失敗時はリソースを逆順に解放してプロセスを終了する（縮退起動なし）
- 堅牢性: TCP切断時の `SIGPIPE` 無視、起動時の IPC キューのパージなど、異常系に対する耐障害性を実装済み。
