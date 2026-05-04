# CLAUDE.md — studyC プロジェクト規約

## モジュール構成

| ディレクトリ | 役割 |
|---|---|
| `Collector` | UDP受信 → SHM書込 + MQ通知 |
| `Router` | MQ受信 → SHM読出 → TCP中継 |
| `Viewer` | TCP受信 → コンソール表示 |
| `SHM` | 共有メモリ API ライブラリ (`libshm.a`) |
| `Common` | 共通ライブラリ：統合ロガー等 (`libcommon.a`) |
| `Mgmt` | 管理基盤ライブラリ：ソケット・ハンドラ登録・プロトコル (`libmgmt.a`) |
| `MgmtCtl` | 管理 CLI：`mgmtctl` バイナリ（`libmgmt.a` をリンク） |

**旧名称（使用禁止）**: PollIO → Collector, TestMsgRcv → Router, TestUDPKill → Viewer

## ビルド

```bash
# 全モジュール（依存順序は Makefile が管理）
make

# デバッグビルド（-DDEBUG フラグ付き）
make debug

# 個別ビルド（依存ライブラリを先にビルドすること）
make -C SHM && make -C Common   # ライブラリ先行
make -C Collector
make -C Router

# クリーン
make clean
```

**依存関係**: ビルド順序は `SHM` → `Common` → `Mgmt` → `Collector` / `Router` / `Viewer` / `MgmtCtl`。
ライブラリは `lib/` 以下に出力される（`libshm.a`, `libcommon.a`, `libmgmt.a`）。

## ロギング規約

統合ロガー (`Common/include/unified_logger.h`) を使用する。

### エントリポイント（main.c）
```c
log_init("ModuleName");   // 起動時（sigaction の後、pthread_create の前）
// ...
log_close();              // 終了時
```

### ログ出力マクロ
```c
// グローバルロガー用（推奨・最もシンプル）
GLOG_TRACE(fmt, ...)
GLOG_DEBUG(fmt, ...)
GLOG_INFO(fmt, ...)
GLOG_WARN(fmt, ...)
GLOG_ERR(fmt, ...)
GLOG_FATAL(fmt, ...)

// ハンドルベース（マルチインスタンス用）
ULOG_DEBUG_AT(handle, fmt, ...)   // __FILE__/__LINE__/__func__ 自動付与
```

**禁止**: `LOG_INFO` / `LOG_ERR` 等のマクロ名は `<syslog.h>` の定数と衝突するため定義・使用しない。

### ログの2層構造

| 層 | マクロ | 出力先 | 有効条件 | 用途 |
|----|--------|--------|---------|------|
| システム層 | `GLOG_*` | syslog | 常時（レベル閾値以上） | 結合レベルの動作記録・障害通知 |
| 単体層 | `DBG(...)` | stderr | `-DDEBUG` ビルド時のみ | モジュール内の変数ダンプ・フロー追跡 |

`DBG` は `Common/include/debug_log.h` が提供する。使用するファイルには以下を書く（`#include "unified_logger.h"` より後）。

```c
#define MODULE_NAME "Worker"   // [DBG][Worker] プレフィックスに使われる
#include "debug_log.h"
```

**方針**: `GLOG_*` に置き換えない。`DBG` はモジュール単体の一時的な観察用であり、`GLOG_*` は恒久的なシステムログ。目的が異なるため共存させる。

### printf を残してよい箇所
- 対話型ターミナルUIのプロンプト（`printf("> "); fflush(stdout);`）
- CLI の usage / unknown option メッセージ（`fprintf(stderr, ...)`）

これらはアプリケーションログではないため `GLOG_*` に変換しない。

## Doxygen 規約

新規作成ファイル（MgmtCtl、各モジュールの `mgmt_handlers.c` 等）に適用する。既存コードへの遡及適用は不要。

### ファイルヘッダ（各 `.c` / `.h` の先頭）
```c
/**
 * @file mgmt_send.c
 * @brief MgmtCtl から各モジュールへコマンドを送受信する
 */
```

### 公開 API（ヘッダに書く）
```c
/**
 * @brief 指定モジュールにコマンドを送信し、応答を受け取る
 * @param socket_path 送信先ソケットパス
 * @param req         送信するコマンドリクエスト
 * @param resp        受信した応答の格納先
 * @return 0 on success, -1 on error
 */
int mgmt_send_command(const char* socket_path,
                      const MgmtCommandRequest* req,
                      MgmtCommandResponse* resp);
```

### 構造体
```c
/** @brief mgmtctl が送受信に使う設定 */
typedef struct {
    const char* socket_path; /**< 送信先ソケットパス */
    int timeout_ms;          /**< recvfrom タイムアウト（ミリ秒） */
} MgmtSendConfig;
```

### 適用しない箇所
- `static` な内部関数（ヘッダに露出しない）
- ファイルスコープのみで使う型・定数
- 既存ファイルへの追記（既存コードはそのまま）

## 静的解析

**Claude はコードを変更してビルドが成功したら、必ず以下を実行してからタスク完了を報告すること。**

```bash
# 変更したモジュールに対して実行（例: Collector を変更した場合）
cppcheck --enable=warning,style --std=c11 -ICollector/src -I lib/include Collector/src/

# 全モジュール一括（チェック内容は .clang-tidy で管理）
bash run_clang_tidy.sh
```

## コーディング規約

- **スレッドセーフ**: `inet_ntoa` は MT-Unsafe なので `inet_ntop` を使用すること
- **標準**: `-std=c11 -D_POSIX_C_SOURCE=200809L`
- **警告**: `-Wall -Wextra` でゼロ警告を維持する

### 初期化失敗時のエラーハンドリング

**すべての初期化は成功した状態でプロセスを起動する。縮退起動は行わない。**

初期化（SHM・MQ・ソケット・mgmt ハンドラ等）が一つでも失敗した場合は、それまでに確保したリソースを解放してプロセスを終了する。

```c
// 全初期化の統一パターン
if (something_init() < 0) {
    GLOG_FATAL("[Module] 初期化失敗: ...");
    // 確保済みリソースを逆順に解放
    log_close();
    return EXIT_FAILURE;
}
```

## 問題発見時のルール

コードレビューや作業中に問題・改善点を発見した場合、**CI が整うまでは即座に修正せず `docs/known_issues.md` に記録する**。

```markdown
## 問題タイトル
- **場所**: ファイルパス:行番号
- **内容**: 何が問題か
- **優先度**: high / medium / low
```

修正は CI（`make test` が通る状態）が整ってから着手する。

## 積み残し（既知の未対応事項）

- `docs/APISpecification.text` に旧モジュール名が残っている（未更新）
- ファイル命名規則の整理（例: `udp_worker.c` → `router_udp_worker.c`）
- Mgmt 統合済み（Phase 2–5 完了）。GET_METRICS / RESET_METRICS ハンドラは未実装（MetricsHandle の組み込みが必要）
